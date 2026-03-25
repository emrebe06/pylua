class LunaraBridge {
  constructor(options = {}) {
    this.apiBase = options.apiBase ?? 'http://127.0.0.1:8093';
    this.wsUrl = options.wsUrl ?? 'ws://127.0.0.1:8094';
    this.ws = null;
    this.wsListeners = [];
  }

  async request(path, options = {}) {
    const response = await fetch(`${this.apiBase}${path}`, {
      credentials: 'include',
      ...options,
      headers: {
        'Content-Type': 'application/json',
        ...(options.headers ?? {})
      }
    });

    const text = await response.text();
    let data = text;
    try {
      data = JSON.parse(text);
    } catch (_) {
    }

    return {
      ok: response.ok,
      status: response.status,
      headers: Object.fromEntries(response.headers.entries()),
      data
    };
  }

  health() {
    return this.request('/health');
  }

  loadUser(id) {
    return this.request(`/api/users/${id}?source=bridge`);
  }

  echo(payload) {
    return this.request('/api/echo', {
      method: 'POST',
      body: JSON.stringify(payload)
    });
  }

  connectSocket() {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      return this.ws;
    }

    this.ws = new WebSocket(this.wsUrl);
    this.ws.addEventListener('message', (event) => {
      let payload = event.data;
      try {
        payload = JSON.parse(event.data);
      } catch (_) {
      }
      for (const listener of this.wsListeners) {
        listener(payload);
      }
    });
    return this.ws;
  }

  onSocketMessage(listener) {
    this.wsListeners.push(listener);
  }

  sendSocketMessage(message) {
    this.connectSocket();
    if (this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(message);
      return;
    }

    this.ws.addEventListener('open', () => this.ws.send(message), { once: true });
  }
}

window.LunaraBridge = LunaraBridge;
