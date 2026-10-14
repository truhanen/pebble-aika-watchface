"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.widgetListToPayload = widgetListToPayload;
// Converts the `widgetList` Clay value (a flat marker-encoded array) into the byte
// payload sent under SettingWidgetList / SettingRightWidgetList. A slot is either a
// plain widget id or a rotating group: 255, count, interval_code, members...
// Drops non-drawable ids/members, clamps count(<=6)/interval(0..5), degrades a
// <2-member group, packs WHOLE groups only, and stops before exceeding 16 bytes.
// KEEP IN SYNC with src/c/widget_list.c.
var MAX_WIDGET_LIST_BYTES = 32; // matches MAX_WIDGET_LIST in src/c/sidebar_widgets.h
var MAX_WIDGET_TYPE = 25; // SLEEP_DEEP_BAR
var CRYPTO_WID_BASE = 200;
var MAX_CRYPTO = 16;
var CURRENCY_WID_BASE = 216;
var MAX_CURRENCY = 7;
var TUYA_WID_BASE = 128;
var MAX_TUYA = 16;
var ROTATING_MARKER = 255;
var MAX_GROUP_MEMBERS = 6;
var WIDGET_HIDE_FLAG = 0x20; // KEEP IN SYNC with src/c/widget_list.h
function isDrawableId(id) {
    var base = id & ~WIDGET_HIDE_FLAG;
    if (base === 0) {
        return false;
    }
    if (base >= 1 && base <= MAX_WIDGET_TYPE) {
        return true;
    }
    if (base >= CRYPTO_WID_BASE && base < CRYPTO_WID_BASE + MAX_CRYPTO) {
        return true;
    }
    if (base >= CURRENCY_WID_BASE && base < CURRENCY_WID_BASE + MAX_CURRENCY) {
        return true;
    }
    if (base >= TUYA_WID_BASE && base < TUYA_WID_BASE + MAX_TUYA) {
        return true;
    }
    return false;
}
function widgetListToPayload(list) {
    var arr = Array.isArray(list) ? list : [];
    var out = [];
    var i = 0;
    while (i < arr.length && out.length < MAX_WIDGET_LIST_BYTES) {
        var head = parseInt(arr[i], 10);
        if (head === ROTATING_MARKER) {
            var count = parseInt(arr[i + 1], 10);
            var intervalRaw = parseInt(arr[i + 2], 10);
            if (isNaN(count) || isNaN(intervalRaw)) {
                break;
            } // malformed tail
            var memStart = i + 3;
            var members = [];
            for (var m = 0; m < count && m < MAX_GROUP_MEMBERS; m++) {
                var id = parseInt(arr[memStart + m], 10);
                if (!isNaN(id) && isDrawableId(id)) {
                    members.push(id);
                }
            }
            i = memStart + count;
            var interval = (intervalRaw >= 0 && intervalRaw <= 5) ? intervalRaw : 3;
            if (members.length >= 2) {
                if (out.length + 3 + members.length > MAX_WIDGET_LIST_BYTES) {
                    break;
                } // whole group only
                out.push(ROTATING_MARKER, members.length, interval);
                for (var m = 0; m < members.length; m++) {
                    out.push(members[m]);
                }
            }
            else if (members.length === 1) {
                out.push(members[0]);
            }
        }
        else {
            if (!isNaN(head) && isDrawableId(head)) {
                out.push(head);
            }
            i += 1;
        }
    }
    return out;
}
