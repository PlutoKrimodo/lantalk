document.addEventListener('DOMContentLoaded', () => {

    function isPasswordValid(pwd) {
        if (pwd.length < 8 || pwd.length > 20) return false;
        if (!/^[a-zA-Z0-9]+$/.test(pwd)) return false;
        return /[a-zA-Z]/.test(pwd) && /[0-9]/.test(pwd);
    }
    function isUsernameValid(name) {
        return /^[a-zA-Z0-9]+$/.test(name);
    }

    const loginForm = document.getElementById('loginForm');
    const registerForm = document.getElementById('registerForm');

    const username = document.getElementById('username');
    const password = document.getElementById('password');
    const err = document.getElementById('err');

    const regUsername = document.getElementById('regUsername');
    const regPassword = document.getElementById('regPassword');
    const regErr = document.getElementById('regErr');

    document.getElementById('showRegister').addEventListener('click', (e) => {
        e.preventDefault();          // 阻止 a 标签跳转
        loginForm.style.display = 'none';
        registerForm.style.display = 'block';
    });

    document.getElementById('showLogin').addEventListener('click', (e) => {
        e.preventDefault();
        registerForm.style.display = 'none';
        loginForm.style.display = 'block';
    });

    // 登录表单提交
    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();   // 阻止页面刷新

        const u = username.value.trim();
        const p = password.value.trim();

        if (!u || !p) {
            err.textContent = '用户名和密码不能为空';
            return;
        }

        try {
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
        } catch (err) {
            err.textContent = '网络异常，请稍后重试';
        }
    });

    // 注册表单提交
    registerForm.addEventListener('submit', async (e) => {
        e.preventDefault();   //阻止页面刷新

        const u = regUsername.value.trim();
        const p = regPassword.value.trim();

        if (!u || !p) {
            regErr.textContent = '用户名和密码不能为空';
            return;
        }

        if (!isUsernameValid(u)) {
            regErr.textContent = '用户名只能包含字母和数字';
            return;
        }

        if (!isPasswordValid(p)) {
            regErr.textContent = '密码需8-20位，仅限字母和数字，且必须同时包含字母和数字';
            return;
        }

        try {
            const resp = await fetch('/register', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'username=' + encodeURIComponent(u) + '&password=' + encodeURIComponent(p)
            });

            const data = await resp.json();

            if (data.ok) {
                regErr.textContent = '注册成功，请登录';
                regErr.style.color = 'green'; 
                regUsername.value = '';
                regPassword.value = '';

                registerForm.style.display = 'none';
                loginForm.style.display = 'block';

                username.value = u;
                password.value = '';
                err.textContent = '';            
            } else {
                regErr.textContent = data.err || '注册失败';
                regErr.style.color = '#d32f2f'; 
            }
        } catch (error) {
            regErr.textContent = '网络异常，请稍后重试';
            regErr.style.color = '#d32f2f';
        }
    });

});