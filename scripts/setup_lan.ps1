$wsl_ip = wsl hostname -I
$wsl_ip = $wsl_ip.Trim()

netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=8888

netsh interface portproxy add v4tov4 `
listenaddress=0.0.0.0 `
listenport=8888 `
connectaddress=$wsl_ip `
connectport=8888

netsh interface portproxy show v4tov4