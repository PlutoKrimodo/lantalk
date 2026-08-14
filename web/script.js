document.addEventListener('DOMContentLoaded', () => {
    // 密码校验函数 长度8-20，仅字母数字，且必须同时包含字母和数字
    function isPasswordValid(pwd) {
        if (pwd.length < 8 || pwd.length > 20) return false;
        if (!/^[a-zA-Z0-9]+$/.test(pwd)) return false;
        return /[a-zA-Z]/.test(pwd) && /[0-9]/.test(pwd);
    }

    // 用户名校验（仅字母数字）
    function isUsernameValid(name) {
        return /^[a-zA-Z0-9]+$/.test(name);
    }

    // 登录相关
    const loginBtn = document.getElementById('loginBtn');
    const username = document.getElementById('username');
    const password = document.getElementById('password');
    const err = document.getElementById('err');

    loginBtn.addEventListener('click', async () => {
        const u = username.value.trim();
        const p = password.value.trim();
        if (!u || !p) {
            err.textContent = '用户名和密码不能为空';
            return;
        }
        const resp = await fetch('/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: 'username=' + encodeURIComponent(u) + '&password=' + encodeURIComponent(p)
        });
        const data = await resp.json();
        if (data.ok) {
            location.href = '/chat.html?token=' + data.token;
        } else {
            err.textContent = '用户名或密码错误';
        }
    });

    // 卡片切换
    const loginCard = document.getElementById('login-card');
    const registerCard = document.getElementById('register-card');
    const showRegister = document.getElementById('showRegister');
    const showLogin = document.getElementById('showLogin');

    showRegister.addEventListener('click', (e) => {
        e.preventDefault();
        loginCard.style.display = 'none';
        registerCard.style.display = 'block';
    });

    showLogin.addEventListener('click', (e) => {
        e.preventDefault();
        registerCard.style.display = 'none';
        loginCard.style.display = 'block';
    });

    // 注册相关
    const regBtn = document.getElementById('regBtn');
    const regUsername = document.getElementById('regUsername');
    const regPassword = document.getElementById('regPassword');
    const regErr = document.getElementById('regErr');

    regBtn.addEventListener('click', async () => {
        const u = regUsername.value.trim();
        const p = regPassword.value.trim();

        // 检查非空
        if (!u || !p) {
            regErr.textContent = '用户名和密码不能为空';
            return;
        }

        // 校验用户名（字母数字）
        if (!isUsernameValid(u)) {
            regErr.textContent = '用户名只能包含字母和数字';
            return;
        }

        // 校验密码（长度8-20，字母数字且同时含字母和数字）
        if (!isPasswordValid(p)) {
            regErr.textContent = '密码需8-20位，仅限字母和数字，且必须同时包含字母和数字';
            return;
        }

        // 发送注册请求
        const resp = await fetch('/register', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: 'username=' + encodeURIComponent(u) + '&password=' + encodeURIComponent(p)
        });

        if (resp.ok) {
            const text = await resp.text();
            if (text.includes('successful')) {
                regErr.textContent = '注册成功，请登录';
                regUsername.value = '';
                regPassword.value = '';
                // 自动切换到登录卡片
                registerCard.style.display = 'none';
                loginCard.style.display = 'block';
                // 自动填入用户名（便于登录）
                username.value = u;
                password.value = '';
                err.textContent = '';
            } else {
                regErr.textContent = text;
            }
        } else {
            const text = await resp.text();
            regErr.textContent = text || '注册失败';
        }
    });
});