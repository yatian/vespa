// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.application.validation.change;

import com.yahoo.config.model.api.ConfigChangeRefeedAction;
import com.yahoo.config.model.api.ServiceInfo;
import com.yahoo.config.application.api.ValidationOverrides;

import java.time.Instant;
import java.util.Collections;
import java.util.List;

/**
 * Represents an action to re-feed a document type in order to handle a config change.
 *
 * @author geirst
 * @author bratseth
 * @since 5.43
 */
public class VespaRefeedAction extends VespaConfigChangeAction implements ConfigChangeRefeedAction {

    /**
     * The name of this action, which must be a valid ValidationId. This is a string here because
     * the validation ids belong to the Vespa model while these names are exposed to the config server,
     * which is model version independent.
     */
    private final String name;

    private final String documentType;
    private final boolean allowed;
    private final Instant now;

    private VespaRefeedAction(String name, String message, List<ServiceInfo> services, String documentType, boolean allowed, Instant now) {
        super(message, services);
        this.name = name;
        this.documentType = documentType;
        this.allowed = allowed;
        this.now = now;
    }

    /** Creates a refeed action with some missing information */
    // TODO: We should require document type or model its absence properly
    public static VespaRefeedAction of(String name, ValidationOverrides overrides, String message, Instant now) {
        return new VespaRefeedAction(name, message, Collections.emptyList(), "", overrides.allows(name, now), now);
    }

    /** Creates a refeed action */
    public static VespaRefeedAction of(String name, ValidationOverrides overrides, String message,
                                       List<ServiceInfo> services, String documentType, Instant now) {
        return new VespaRefeedAction(name, message, services, documentType, overrides.allows(name, now), now);
    }

    @Override
    public VespaConfigChangeAction modifyAction(String newMessage, List<ServiceInfo> newServices, String documentType) {
        return new VespaRefeedAction(name, newMessage, newServices, documentType, allowed, now);
    }

    @Override
    public String name() { return name; }

    @Override
    public String getDocumentType() { return documentType; }

    @Override
    public boolean allowed() { return allowed; }

    @Override
    public String toString() {
        return super.toString() + ", documentType='" + documentType + "'";
    }

    @Override
    public boolean equals(Object o) {
        if ( ! super.equals(o)) return false;
        if ( ! (o instanceof VespaRefeedAction)) return false;
        VespaRefeedAction other = (VespaRefeedAction)o;
        if ( ! this.documentType.equals(other.documentType)) return false;
        if ( ! this.name.equals(other.name)) return false;
        if ( ! this.allowed == other.allowed) return false;
        return true;
    }

    @Override
    public int hashCode() {
        return 31 * super.hashCode() + 11 * name.hashCode() + documentType.hashCode();
    }

}
