const output = document.getElementById('output');

async function boot() {
  const health = await fetch('/health').then((response) => response.json());
  const socket = new WebSocket(`ws://${location.host}/ws`);

  socket.addEventListener('open', () => socket.send('hello-from-browser'));
  socket.addEventListener('message', (event) => {
    output.textContent = JSON.stringify({
      health,
      websocket: JSON.parse(event.data)
    }, null, 2);
  });
}

boot().catch((error) => {
  output.textContent = String(error);
});
