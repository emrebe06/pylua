const output = document.getElementById('output');

async function call(path, options = {}) {
  const response = await fetch(path, {
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

  output.textContent = JSON.stringify({ status: response.status, data }, null, 2);
}

document.getElementById('register-btn').addEventListener('click', () => {
  call('/register', {
    method: 'POST',
    body: JSON.stringify({ username: 'demo', password: 'demo-pass' })
  });
});

document.getElementById('login-btn').addEventListener('click', () => {
  call('/login', {
    method: 'POST',
    body: JSON.stringify({ username: 'demo', password: 'demo-pass' })
  });
});

document.getElementById('me-btn').addEventListener('click', () => {
  call('/me');
});

document.getElementById('protected-btn').addEventListener('click', () => {
  call('/protected');
});

document.getElementById('logout-btn').addEventListener('click', () => {
  call('/logout', { method: 'POST' });
});
