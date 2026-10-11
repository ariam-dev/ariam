// Copyright (c) 2026 dta0one
// SPDX-License-Identifier: MIT
//
// The browser supplies the page's Basic credentials; no token is ever sent.

import { mount, TransportError } from './queue-view.js';

const ENDPOINT = '/jsonrpc';
let nextId = 0;

async function post(method, params) {
  nextId += 1;
  let response;
  try {
    response = await fetch(ENDPOINT, {
      method: 'POST',
      credentials: 'same-origin',
      cache: 'no-store',
      headers: { 'Content-Type': 'application/json', 'X-Ariam-Page': '1' },
      body: JSON.stringify({ jsonrpc: '2.0', id: nextId, method, params }),
    });
  } catch (err) {
    throw new TransportError('network', 'network error');
  }
  // Lockout: empty body, no passphrase check.
  if (response.status === 429) {
    throw new TransportError('locked', 'too many attempts', { status: 429 });
  }
  let text;
  try {
    text = await response.text();
  } catch (err) {
    throw new TransportError('network', 'network error');
  }
  let data = null;
  try {
    data = text === '' ? null : JSON.parse(text);
  } catch (err) {
    data = null;
  }
  if (data && typeof data === 'object' && data.error && typeof data.error === 'object') {
    const message = typeof data.error.message === 'string' ? data.error.message : 'error';
    throw new TransportError('rpc', message, { status: response.status, code: Number(data.error.code) });
  }
  if (!response.ok || !data || typeof data !== 'object' || !('result' in data)) {
    throw new TransportError('http', `HTTP ${response.status}`, { status: response.status });
  }
  return data.result;
}

const transport = {
  call(method, params) {
    return post(method, Array.isArray(params) ? params : []);
  },
  multicall(calls) {
    return post('system.multicall', [calls.map((c) => ({ methodName: c.methodName, params: c.params }))]);
  },
};

mount(document.getElementById('app'), transport);
