(function() {
    // 获取 token
    const params = new URLSearchParams(location.search);
    const token = params.get("token");
    const statusEl = document.getElementById('status');
    const msgs = document.getElementById('msgs');
    const textInput = document.getElementById('text');
    const sendBtn = document.getElementById('send');

    if (!token) {
        statusEl.textContent = '缺少 token，请重新登录';
        document.getElementById('inputbar').style.display = 'none';
        return;
    }

    let ws = null;

    // 渲染消息
    function renderMsg(m) {
        const div = document.createElement('div');
        div.className = 'msg' + (m.self ? ' self' : '');

        const meta = document.createElement('span');
        meta.className = 'meta';
        meta.textContent = (m.from || '系统') + ' ' + (m.ts || '');

        const content = document.createElement('span');
        content.className = 'content';
        content.textContent = m.content;   

        div.appendChild(meta);
        div.appendChild(content);
        msgs.appendChild(div);
        msgs.scrollTop = msgs.scrollHeight;
    }

    // 发送消息
    function sendMessage() {
        const text = textInput.value.trim();
        if (!text || !ws || ws.readyState !== WebSocket.OPEN) {
            return;
        }
        ws.send(JSON.stringify({
            type: 'msg',
            to: 0,          // 群聊标识（目前写死）
            content: text
        }));
        textInput.value = '';
        textInput.focus();
    }

    // 连接 WebSocket
    function connect() {
        const wsProtocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
        ws = new WebSocket(wsProtocol + '//' + location.host + '/chat');

        ws.onopen = function() {
            statusEl.textContent = '已连接';
            statusEl.style.background = '#d4edda';
            ws.send(JSON.stringify({ type: 'auth', token }));
        };

        ws.onmessage = function(e) {
            try {
                const m = JSON.parse(e.data);
                console.log('收到消息:', m);

                if (m.type === 'auth') {
                    if (m.ok) {
                        statusEl.textContent = '已认证';
                        statusEl.style.background = '#d4edda';
                    } else {
                        statusEl.textContent = '认证失败: ' + (m.error || '');
                        statusEl.style.background = '#f8d7da';
                        textInput.disabled = true;
                        sendBtn.disabled = true;
                    }
                } else if (m.type === 'msg') {
                    renderMsg(m);
                } else {
                    console.warn('未知消息类型:', m.type);
                }
            } catch (err) {
                console.error('解析消息失败:', err);
            }
        };

        ws.onclose = function() {
            statusEl.textContent = '连接已断开，尝试重连...';
            statusEl.style.background = '#ffeaa7';
            setTimeout(connect, 5000);
        };

        ws.onerror = function(err) {
            console.error('WebSocket 错误:', err);
        };
    }

    // 绑定发送事件
    sendBtn.addEventListener('click', sendMessage);

    textInput.addEventListener('keydown', function(e) {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });

    // 启动 
    connect();

    // 页面关闭时主动断开
    window.addEventListener('beforeunload', function() {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.close();
        }
    });
})();