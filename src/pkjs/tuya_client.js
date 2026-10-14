"use strict";
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
Object.defineProperty(exports, "__esModule", { value: true });
exports.EMPTY_BODY_SHA256 = void 0;
exports.buildStringToSign = buildStringToSign;
exports.buildSignString = buildSignString;
exports.sign = sign;
exports.createClient = createClient;
/* Tuya cloud auth + request signing, lifted from PebbleTuyaControl's tuya-client.js.
   Pure + dependency-injected: the caller supplies `http` (a Promise-returning XHR
   wrapper that resolves the parsed Tuya envelope) and `deps` {now, nonce, loadToken?,
   saveToken?}. Only dependency is js-sha256 (pure JS, PKJS-safe). Unit-tested in
   tests/tuya_client.test.js. */
var js_sha256_1 = require("js-sha256");
exports.EMPTY_BODY_SHA256 = 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855';
// urlPath includes the query string already (sorted by caller when present).
function buildStringToSign(method, urlPath, bodyStr) {
    var contentHash = (bodyStr && bodyStr.length) ? (0, js_sha256_1.sha256)(bodyStr) : exports.EMPTY_BODY_SHA256;
    return method + '\n' + contentHash + '\n' + '\n' + urlPath; // empty Signature-Headers line
}
function buildSignString(o) {
    var head = o.accessToken ? (o.clientId + o.accessToken) : o.clientId;
    return head + o.t + o.nonce + o.stringToSign;
}
function sign(signString, secret) {
    return js_sha256_1.sha256.hmac(secret, signString).toUpperCase();
}
function createClient(cfg, http, deps) {
    var token = null;
    var tokenExpiresAt = 0;
    if (deps.loadToken) {
        var saved = deps.loadToken(cfg.clientId);
        if (saved && saved.token && deps.now() < saved.expiresAt) {
            token = saved.token;
            tokenExpiresAt = saved.expiresAt;
        }
    }
    function headersFor(method, urlPath, bodyStr, accessToken) {
        var t = String(deps.now());
        var nonce = deps.nonce();
        var sts = buildStringToSign(method, urlPath, bodyStr);
        var signStr = buildSignString({
            clientId: cfg.clientId, accessToken: accessToken, t: t, nonce: nonce, stringToSign: sts,
        });
        var h = {
            client_id: cfg.clientId,
            sign: sign(signStr, cfg.secret),
            sign_method: 'HMAC-SHA256',
            t: t,
            nonce: nonce,
            'Content-Type': 'application/json',
        };
        if (accessToken) {
            h.access_token = accessToken;
        }
        return h;
    }
    function getToken() {
        var urlPath = '/v1.0/token?grant_type=1';
        return http({ method: 'GET', url: cfg.host + urlPath, headers: headersFor('GET', urlPath, '', null) })
            .then(function (resp) {
            if (!resp || !resp.success) {
                throw new Error('token error ' + (resp && resp.code));
            }
            token = resp.result.access_token;
            tokenExpiresAt = deps.now() + (resp.result.expire_time - 60) * 1000;
            if (deps.saveToken) {
                deps.saveToken(cfg.clientId, { token: token, expiresAt: tokenExpiresAt });
            }
            return token;
        });
    }
    function ensureToken() {
        if (token && deps.now() < tokenExpiresAt) {
            return Promise.resolve(token);
        }
        return getToken();
    }
    var AUTH_ERROR_CODES = [1010, 1011, 1012, 1013];
    function request(method, urlPath, body, _retried) {
        var bodyStr = body ? JSON.stringify(body) : '';
        return ensureToken().then(function (tok) {
            return http({ method: method, url: cfg.host + urlPath, body: bodyStr,
                headers: headersFor(method, urlPath, bodyStr, tok) });
        }).then(function (resp) {
            if (!resp || !resp.success) {
                if (!_retried && resp && AUTH_ERROR_CODES.indexOf(resp.code) >= 0) {
                    token = null;
                    tokenExpiresAt = 0;
                    return request(method, urlPath, body, true);
                }
                var e = new Error('Tuya API error ' + (resp && resp.code) + ': ' + (resp && resp.msg));
                e.code = resp && resp.code;
                throw e;
            }
            return resp;
        });
    }
    return { getToken: getToken, request: request };
}
