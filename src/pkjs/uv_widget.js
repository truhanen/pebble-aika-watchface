"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.UV_WIDGET_ID = void 0;
exports.isUvWidgetConfigured = isUvWidgetConfigured;
/* Whether the UV-index widget (id 13) is currently placed in either sidebar
   column. Read straight from Clay's persisted `clay-settings` (the same store the
   widgetList migration reads), so it always reflects the user's current layout
   without a separate flag to keep in sync. Used by the FMI weather provider to
   decide whether to make the extra radiation-observation request for UV -- FMI's
   forecast carries no UV, so we only pay for the UV obs query when the widget that
   shows it is actually installed. */
var widget_list_payload_1 = require("./widget_list_payload");
exports.UV_WIDGET_ID = 13; // matches widget_options.ts / sidebar_widgets.h
function isUvWidgetConfigured(storage) {
    var stored;
    try {
        stored = JSON.parse(storage.getItem('clay-settings') || '{}') || {};
    }
    catch (e) {
        return false;
    }
    var ids = (0, widget_list_payload_1.widgetListToPayload)(stored.WidgetList)
        .concat((0, widget_list_payload_1.widgetListToPayload)(stored.WidgetListRight));
    return ids.indexOf(exports.UV_WIDGET_ID) !== -1;
}
