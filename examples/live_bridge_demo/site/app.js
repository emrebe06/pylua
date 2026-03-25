const httpOutput = document.getElementById('http-output');
const wsOutput = document.getElementById('ws-output');
const statusPill = document.getElementById('status-pill');
const eventLog = document.getElementById('event-log');

const bridge = new window.LunaraBridge();

function writeHttp(payload) {
  httpOutput.textContent = JSON.stringify(payload, null, 2);
}

function writeWs(payload) {
  const next = typeof payload === 'string' ? payload : JSON.stringify(payload, null, 2);
  wsOutput.textContent = `${wsOutput.textContent}\n${next}`.trim();
}

function logEvent(text) {
  const item = document.createElement('li');
  item.textContent = text;
  eventLog.prepend(item);
}

function setReadyState(text, ready) {
  statusPill.textContent = text;
  statusPill.className = ready ? 'pill ready' : 'pill pending';
}

bridge.onSocketMessage((payload) => {
  setReadyState('socket baglandi', true);
  writeWs(payload);
  logEvent(`WebSocket veri alindi: ${typeof payload === 'string' ? payload : payload.kind ?? 'json'}`);
});

bridge.connectSocket();

document.getElementById('health-btn').addEventListener('click', async () => {
  const result = await bridge.health();
  writeHttp(result);
  logEvent(`Health check tamamlandi (${result.status})`);
});

document.getElementById('user-btn').addEventListener('click', async () => {
  const result = await bridge.loadUser(42);
  writeHttp(result);
  logEvent('Route param ile kullanici verisi yuklendi');
});

document.getElementById('echo-btn').addEventListener('click', async () => {
  const result = await bridge.echo({
    channel: 'browser',
    message: 'merhaba lunara',
    sent_at: new Date().toISOString()
  });
  writeHttp(result);
  logEvent('Cross-origin POST echo tamamlandi');
});

document.getElementById('ws-btn').addEventListener('click', () => {
  bridge.sendSocketMessage('frontend mesaji ' + new Date().toISOString());
  logEvent('WebSocket mesaji gonderildi');
});

setReadyState('bridge hazir', false);
