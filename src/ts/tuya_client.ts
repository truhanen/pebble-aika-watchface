// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Tuya cloud auth + request signing, lifted from PebbleTuyaControl's tuya-client.js.
   Pure + dependency-injected: the caller supplies `http` (a Promise-returning XHR
   wrapper that resolves the parsed Tuya envelope) and `deps` {now, nonce, loadToken?,
   saveToken?}. Only dependency is js-sha256 (pure JS, PKJS-safe). Unit-tested in
   tests/tuya_client.test.js. */

import { sha256 } from 'js-sha256';

export const EMPTY_BODY_SHA256 =
  'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855';

// urlPath includes the query string already (sorted by caller when present).
export function buildStringToSign(method: string, urlPath: string, bodyStr: string): string {
  const contentHash = (bodyStr && bodyStr.length) ? sha256(bodyStr) : EMPTY_BODY_SHA256;
  return method + '\n' + contentHash + '\n' + '\n' + urlPath;   // empty Signature-Headers line
}

export function buildSignString(o: any): string {
  const head = o.accessToken ? (o.clientId + o.accessToken) : o.clientId;
  return head + o.t + o.nonce + o.stringToSign;
}

export function sign(signString: string, secret: string): string {
  return sha256.hmac(secret, signString).toUpperCase();
}

export function createClient(cfg: any, http: any, deps: any): any {
  let token: string | null = null;
  let tokenExpiresAt = 0;

  if (deps.loadToken) {
    const saved = deps.loadToken(cfg.clientId);
    if (saved && saved.token && deps.now() < saved.expiresAt) {
      token = saved.token; tokenExpiresAt = saved.expiresAt;
    }
  }

  function headersFor(method: string, urlPath: string, bodyStr: string, accessToken: string | null): any {
    const t = String(deps.now());
    const nonce = deps.nonce();
    const sts = buildStringToSign(method, urlPath, bodyStr);
    const signStr = buildSignString({
      clientId: cfg.clientId, accessToken: accessToken, t: t, nonce: nonce, stringToSign: sts,
    });
    const h: any = {
      client_id: cfg.clientId,
      sign: sign(signStr, cfg.secret),
      sign_method: 'HMAC-SHA256',
      t: t,
      nonce: nonce,
      'Content-Type': 'application/json',
    };
    if (accessToken) { h.access_token = accessToken; }
    return h;
  }

  function getToken(): Promise<string> {
    const urlPath = '/v1.0/token?grant_type=1';
    return http({ method: 'GET', url: cfg.host + urlPath, headers: headersFor('GET', urlPath, '', null) })
      .then((resp: any) => {
        if (!resp || !resp.success) { throw new Error('token error ' + (resp && resp.code)); }
        token = resp.result.access_token;
        tokenExpiresAt = deps.now() + (resp.result.expire_time - 60) * 1000;
        if (deps.saveToken) { deps.saveToken(cfg.clientId, { token: token, expiresAt: tokenExpiresAt }); }
        return token as string;
      });
  }

  function ensureToken(): Promise<string> {
    if (token && deps.now() < tokenExpiresAt) { return Promise.resolve(token); }
    return getToken();
  }

  const AUTH_ERROR_CODES = [1010, 1011, 1012, 1013];

  function request(method: string, urlPath: string, body?: any, _retried?: boolean): Promise<any> {
    const bodyStr = body ? JSON.stringify(body) : '';
    return ensureToken().then((tok) =>
      http({ method: method, url: cfg.host + urlPath, body: bodyStr,
             headers: headersFor(method, urlPath, bodyStr, tok) })
    ).then((resp: any) => {
      if (!resp || !resp.success) {
        if (!_retried && resp && AUTH_ERROR_CODES.indexOf(resp.code) >= 0) {
          token = null; tokenExpiresAt = 0;
          return request(method, urlPath, body, true);
        }
        const e: any = new Error('Tuya API error ' + (resp && resp.code) + ': ' + (resp && resp.msg));
        e.code = resp && resp.code;
        throw e;
      }
      return resp;
    });
  }

  return { getToken: getToken, request: request };
}
