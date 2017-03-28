// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "attribute_initializer.h"
#include "attributedisklayout.h"
#include "attribute_directory.h"
#include <vespa/searchcore/proton/common/eventlogger.h>
#include <vespa/vespalib/data/fileheader.h>
#include <vespa/vespalib/io/fileutil.h>
#include <vespa/searchlib/util/fileutil.h>
#include <vespa/searchlib/attribute/attribute_header.h>
#include <vespa/fastos/file.h>

#include <vespa/log/log.h>
LOG_SETUP(".proton.attribute.attribute_initializer");

using search::attribute::BasicType;
using search::attribute::Config;
using search::AttributeVector;
using search::IndexMetaInfo;

namespace proton {

using search::attribute::AttributeHeader;

namespace {

vespalib::string
extraPredicateType(const search::attribute::PersistentPredicateParams &params)
{
    vespalib::asciistream os;
    os << "arity=" << params.arity();
    os << ",lower_bound=" << params.lower_bound();
    os << ",upper_bound=" << params.upper_bound();
    return os.str();
}

vespalib::string
extraType(const Config &cfg)
{
    if (cfg.basicType().type() == BasicType::Type::TENSOR) {
        return cfg.tensorType().to_spec();
    }
    if (cfg.basicType().type() == BasicType::Type::PREDICATE) {
        return extraPredicateType(cfg.predicateParams());
    }
    return "";
}

vespalib::string
extraType(const AttributeHeader &header)
{
    if (header.getBasicType().type() == BasicType::Type::TENSOR) {
        return header.getTensorType().to_spec();
    }
    if (header.getBasicType().type() == BasicType::Type::PREDICATE) {
        return extraPredicateType(header.getPredicateParams());
    }
    return "";
}

bool
headerTypeOK(const AttributeHeader &header, const Config &cfg)
{
    if ((header.getBasicType().type() != cfg.basicType().type()) ||
        (header.getCollectionType().type() != cfg.collectionType().type())) {
        return false;
    }
    if (cfg.basicType().type() == BasicType::Type::TENSOR) {
        if (header.getTensorType() != cfg.tensorType()) {
            return false;
        }
    }
    if (cfg.basicType().type() == BasicType::PREDICATE) {
        if (header.getPredicateParamsSet()) {
            if (!(header.getPredicateParams() == cfg.predicateParams())) {
                return false;
            }
        }
    }
    return true;
}

AttributeHeader
extractHeader(const vespalib::string &attrFileName)
{
    auto df = search::FileUtil::openFile(attrFileName + ".dat");
    vespalib::FileHeader datHeader;
    datHeader.readFile(*df);
    AttributeHeader retval;
    retval.extractTags(datHeader);
    return retval;
}

void
logAttributeTooNew(const AttributeVector::SP &attr,
                   const AttributeHeader &header,
                   uint64_t serialNum)
{
    LOG(info, "Attribute vector '%s' is too new (%" PRIu64 " > %" PRIu64 ")",
        attr->getBaseFileName().c_str(),
        header.getCreateSerialNum(),
        serialNum);
}

void
logAttributeWrongType(const AttributeVector::SP &attr,
                      const AttributeHeader &header)
{
    const Config &cfg(attr->getConfig());
    vespalib::string extraCfgType = extraType(cfg);
    vespalib::string extraHeaderType = extraType(header);
    LOG(info, "Attribute vector '%s' is of wrong type (expected %s/%s/%s, got %s/%s/%s)",
            attr->getBaseFileName().c_str(),
            cfg.basicType().asString(),
            cfg.collectionType().asString(),
            extraCfgType.c_str(),
            header.getBasicType().asString(),
            header.getCollectionType().asString(),
            extraHeaderType.c_str());
}

}

AttributeVector::SP
AttributeInitializer::tryLoadAttribute() const
{
    search::SerialNum serialNum = _attrDir->getFlushedSerialNum();
    vespalib::string attrFileName = _attrDir->getAttributeFileName(serialNum);
    AttributeVector::SP attr = _factory.create(attrFileName, _cfg);
    if (serialNum != 0) {
        AttributeHeader header = extractHeader(attrFileName);
        if (header.getCreateSerialNum() > _currentSerialNum || !headerTypeOK(header, attr->getConfig())) {
            setupEmptyAttribute(attr, serialNum, header);
            return attr;
        }
        if (!loadAttribute(attr, serialNum)) {
            return AttributeVector::SP();
        }
    } else {
        _factory.setupEmpty(attr, _currentSerialNum);
    }
    return attr;
}

bool
AttributeInitializer::loadAttribute(const AttributeVector::SP &attr,
                                    search::SerialNum serialNum) const
{
    assert(attr->hasLoadData());
    fastos::TimeStamp startTime = fastos::ClockSystem::now();
    EventLogger::loadAttributeStart(_documentSubDbName, attr->getName());
    if (!attr->load()) {
        LOG(warning, "Could not load attribute vector '%s' from disk. "
                "Returning empty attribute vector",
                attr->getBaseFileName().c_str());
        return false;
    } else {
        attr->commit(serialNum, serialNum);
        fastos::TimeStamp endTime = fastos::ClockSystem::now();
        int64_t elapsedTimeMs = (endTime - startTime).ms();
        EventLogger::loadAttributeComplete(_documentSubDbName, attr->getName(), elapsedTimeMs);
    }
    return true;
}

void
AttributeInitializer::setupEmptyAttribute(AttributeVector::SP &attr,
                                          search::SerialNum serialNum,
                                          const AttributeHeader &header) const
{
    if (header.getCreateSerialNum() > _currentSerialNum) {
        logAttributeTooNew(attr, header, _currentSerialNum);
    }
    if (!headerTypeOK(header, attr->getConfig())) {
        logAttributeWrongType(attr, header);
    }
    LOG(info, "Returning empty attribute vector for '%s'",
            attr->getBaseFileName().c_str());
    _factory.setupEmpty(attr, _currentSerialNum);
    attr->commit(serialNum, serialNum);
}

AttributeVector::SP
AttributeInitializer::createAndSetupEmptyAttribute() const
{
    vespalib::string attrFileName = _attrDir->getAttributeFileName(0);
    AttributeVector::SP attr = _factory.create(attrFileName, _cfg);
    _factory.setupEmpty(attr, _currentSerialNum);
    return attr;
}

AttributeInitializer::AttributeInitializer(const std::shared_ptr<AttributeDirectory> &attrDir,
                                           const vespalib::string &documentSubDbName,
                                           const search::attribute::Config &cfg,
                                           uint64_t currentSerialNum,
                                           const IAttributeFactory &factory)
    : _attrDir(attrDir),
      _documentSubDbName(documentSubDbName),
      _cfg(cfg),
      _currentSerialNum(currentSerialNum),
      _factory(factory)
{
}

AttributeInitializer::~AttributeInitializer() {}

search::AttributeVector::SP
AttributeInitializer::init() const
{
    if (!_attrDir->empty()) {
        return tryLoadAttribute();
    } else {
        return createAndSetupEmptyAttribute();
    }
}

} // namespace proton
