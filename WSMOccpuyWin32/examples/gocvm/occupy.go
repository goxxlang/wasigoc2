package main

import (
	"fmt"
	"gocvm"
	"strings"
)

// Occupancy guest for the Win32 hop. The operator board Calls this hop
// and waits; it does not own the payload. Isolation is thin at host /
// sys / ipc / compositor / hv / tty / net.

func main() {
	fmt.Println("guest=occupy via=win32")

	st, err := gocvm.Call("win32.occupyWin32", "")
	if err != nil {
		fmt.Println("occupy=fail")
		return
	}
	if strings.Contains(st, `"thin":true`) {
		fmt.Println("occupy=ok")
	} else {
		fmt.Println("occupy=fail")
		return
	}

	cat, err := gocvm.Call("win32.occupyCatalog", "")
	if err != nil {
		fmt.Println("catalog=fail")
		return
	}
	if strings.Contains(cat, `"catalog":true`) {
		fmt.Println("catalog=ok")
	} else {
		fmt.Println("catalog=fail")
		return
	}

	k32, err := gocvm.Call("win32.occupyKernel32", "")
	if err != nil {
		fmt.Println("kernel32=fail")
		return
	}
	if strings.Contains(k32, `"isolation":"host"`) {
		fmt.Println("kernel32=ok isolation=host")
	} else {
		fmt.Println("kernel32=fail")
		return
	}

	nt, err := gocvm.Call("win32.occupyNtdll", "")
	if err != nil {
		fmt.Println("ntdll=fail")
		return
	}
	if strings.Contains(nt, `"isolation":"sys"`) {
		fmt.Println("ntdll=ok isolation=sys")
	} else {
		fmt.Println("ntdll=fail")
		return
	}

	us, err := gocvm.Call("win32.occupyUser32", "")
	if err != nil {
		fmt.Println("user32=fail")
		return
	}
	if strings.Contains(us, `"isolation":"compositor"`) {
		fmt.Println("user32=ok isolation=compositor")
	} else {
		fmt.Println("user32=fail")
		return
	}

	gd, err := gocvm.Call("win32.occupyGdi32", "")
	if err != nil {
		fmt.Println("gdi32=fail")
		return
	}
	if strings.Contains(gd, `"isolation":"compositor"`) {
		fmt.Println("gdi32=ok isolation=compositor")
	} else {
		fmt.Println("gdi32=fail")
		return
	}

	ol, err := gocvm.Call("win32.occupyOle32", "")
	if err != nil {
		fmt.Println("ole32=fail")
		return
	}
	if strings.Contains(ol, `"isolation":"ipc"`) {
		fmt.Println("ole32=ok isolation=ipc")
	} else {
		fmt.Println("ole32=fail")
		return
	}

	cm, err := gocvm.Call("win32.occupyCom", "")
	if err != nil {
		fmt.Println("com=fail")
		return
	}
	if strings.Contains(cm, "DuplicateTokenEx") && strings.Contains(cm, "CoCreateInstance") && strings.Contains(cm, "CoInitializeEx") && strings.Contains(cm, "ImpersonateLoggedOnUser") && strings.Contains(cm, "RevertToSelf") && !strings.Contains(cm, "unknown api ImpersonateLoggedOnUser") && !strings.Contains(cm, "unknown api CoCreateInstance") {
		fmt.Println("com=ok isolation=ipc ImpersonateLoggedOnUser RevertToSelf")
	} else {
		fmt.Println("com=fail")
		return
	}

	ad, err := gocvm.Call("win32.occupyAdvapi32", "")
	if err != nil {
		fmt.Println("advapi32=fail")
		return
	}
	if strings.Contains(ad, `"isolation":"sys"`) {
		fmt.Println("advapi32=ok isolation=sys")
	} else {
		fmt.Println("advapi32=fail")
		return
	}

	tk, err := gocvm.Call("win32.occupyToken", "")
	if err != nil {
		fmt.Println("token=fail")
		return
	}
	if strings.Contains(tk, "SeLoadDriverPrivilege") && strings.Contains(tk, "SeChangeNotifyPrivilege") && strings.Contains(tk, "LookupPrivilegeValueW") && strings.Contains(tk, "ImpersonateLoggedOnUser") && strings.Contains(tk, "RevertToSelf") && strings.Contains(tk, "OpenSCManagerW") && !strings.Contains(tk, "unknown api ImpersonateLoggedOnUser") && !strings.Contains(tk, "unknown api OpenSCManagerW") {
		fmt.Println("token=ok isolation=sys ImpersonateLoggedOnUser OpenSCManagerW")
	} else {
		fmt.Println("token=fail")
		return
	}

	rg, err := gocvm.Call("win32.occupyRegistry", "")
	if err != nil {
		fmt.Println("registry=fail")
		return
	}
	if strings.Contains(rg, "RegCreateKeyExW") && strings.Contains(rg, "CurrentControlSet") && strings.Contains(rg, `"isolation":"sys"`) {
		fmt.Println("registry=ok isolation=sys RegCreateKeyExW")
	} else {
		fmt.Println("registry=fail")
		return
	}

	scm, err := gocvm.Call("win32.occupyScm", "")
	if err != nil {
		fmt.Println("scm=fail")
		return
	}
	if strings.Contains(scm, "OpenSCManagerW") && strings.Contains(scm, "QueryServiceStatus") && !strings.Contains(scm, "unknown api OpenSCManagerW") {
		fmt.Println("scm=ok isolation=sys OpenSCManagerW")
	} else {
		fmt.Println("scm=fail")
		return
	}

	sk, err := gocvm.Call("win32.occupySockets", "")
	if err != nil {
		fmt.Println("sockets=fail")
		return
	}
	if strings.Contains(sk, `"isolation":"net"`) {
		fmt.Println("sockets=ok isolation=net")
	} else {
		fmt.Println("sockets=fail")
		return
	}

	bc, err := gocvm.Call("win32.occupyBcrypt", "")
	if err != nil {
		fmt.Println("bcrypt=fail")
		return
	}
	if strings.Contains(bc, `"isolation":"host"`) {
		fmt.Println("bcrypt=ok isolation=host")
	} else {
		fmt.Println("bcrypt=fail")
		return
	}

	vm, err := gocvm.Call("win32.occupyVmem", "")
	if err != nil {
		fmt.Println("vmem=fail")
		return
	}
	if strings.Contains(vm, `"isolation":"host"`) && strings.Contains(vm, `"mapped":true`) {
		fmt.Println("vmem=ok isolation=host mapped=true")
	} else {
		fmt.Println("vmem=fail")
		return
	}

	pp, err := gocvm.Call("win32.occupyPipe", "")
	if err != nil {
		fmt.Println("pipe=fail")
		return
	}
	if strings.Contains(pp, `"isolation":"ipc"`) {
		fmt.Println("pipe=ok isolation=ipc")
	} else {
		fmt.Println("pipe=fail")
		return
	}

	pe, err := gocvm.Call("win32.occupyPe", "")
	if err != nil {
		fmt.Println("pe=fail")
		return
	}
	if strings.Contains(pe, `"isolation":"host"`) {
		fmt.Println("pe=ok isolation=host")
	} else {
		fmt.Println("pe=fail")
		return
	}

	pr, err := gocvm.Call("win32.occupyProcess", "")
	if err != nil {
		fmt.Println("process=fail")
		return
	}
	if strings.Contains(pr, `"isolation":"shell"`) {
		fmt.Println("process=ok isolation=shell")
	} else {
		fmt.Println("process=fail")
		return
	}

	cn, err := gocvm.Call("win32.occupyConsole", "")
	if err != nil {
		fmt.Println("console=fail")
		return
	}
	if strings.Contains(cn, `"isolation":"tty"`) {
		fmt.Println("console=ok isolation=tty")
	} else {
		fmt.Println("console=fail")
		return
	}

	hw, err := gocvm.Call("win32.occupyHwnd", "")
	if err != nil {
		fmt.Println("hwnd=fail")
		return
	}
	if strings.Contains(hw, `"isolation":"compositor"`) {
		fmt.Println("hwnd=ok isolation=compositor")
	} else {
		fmt.Println("hwnd=fail")
		return
	}

	hv, err := gocvm.Call("win32.occupyHv", "")
	if err != nil {
		fmt.Println("hv=fail")
		return
	}
	if strings.Contains(hv, `"isolation":"hv"`) {
		fmt.Println("hv=ok isolation=hv")
	} else {
		fmt.Println("hv=fail")
		return
	}

	ws, err := gocvm.Call("win32.occupyWsl", "")
	if err != nil {
		fmt.Println("wsl=fail")
		return
	}
	if strings.Contains(ws, `"isolation":"tty"`) {
		fmt.Println("wsl=ok isolation=tty")
	} else {
		fmt.Println("wsl=fail")
		return
	}

	thin, err := gocvm.Call("win32.thinMap", "")
	if err != nil {
		fmt.Println("thin=fail")
		return
	}
	if strings.Contains(thin, `"thin":true`) && strings.Contains(thin, "host") && strings.Contains(thin, "sys") && strings.Contains(thin, "ipc") && strings.Contains(thin, "hv") {
		fmt.Println("thin=true")
	} else {
		fmt.Println("thin=fail")
		return
	}

	sy, err := gocvm.Call("win32.occupySys", "")
	if err != nil {
		fmt.Println("sys=fail")
		return
	}
	if strings.Contains(sy, `"isolation":"sys"`) {
		fmt.Println("sys=ok isolation=sys")
	} else {
		fmt.Println("sys=fail")
		return
	}

	drv, err := gocvm.Call("win32.occupyDriver", "")
	if err != nil {
		fmt.Println("driver=fail")
		return
	}
	if strings.Contains(drv, "wowwin32.sys") && strings.Contains(drv, `"isolation":"sys"`) {
		fmt.Println("driver=ok isolation=sys")
	} else {
		fmt.Println("driver=fail")
		return
	}

	ld, err := gocvm.Call("win32.occupyLoadDriver", "")
	if err != nil {
		fmt.Println("loaddriver=fail")
		return
	}
	if strings.Contains(ld, "NtLoadDriver") && strings.Contains(ld, "ZwLoadDriver") && !strings.Contains(ld, "unknown api NtLoadDriver") {
		fmt.Println("loaddriver=ok isolation=sys NtLoadDriver")
	} else {
		fmt.Println("loaddriver=fail")
		return
	}

	dv, err := gocvm.Call("win32.occupyDevice", "")
	if err != nil {
		fmt.Println("device=fail")
		return
	}
	if strings.Contains(dv, "CreateFileW") && strings.Contains(dv, "WowWin32") {
		fmt.Println("device=ok isolation=host CreateFileW")
	} else {
		fmt.Println("device=fail")
		return
	}

	info, err := gocvm.Call("win32.getDriverInfo", "")
	if err != nil {
		fmt.Println("driverInfo=fail")
		return
	}
	if strings.Contains(info, "wowwin32.sys") && strings.Contains(info, "ntoskrnl.exe") {
		fmt.Println("driverInfo=ok image=wowwin32.sys")
	} else {
		fmt.Println("driverInfo=fail")
		return
	}

	hb, err := gocvm.Call("win32.occupyHostNtos", "")
	if err != nil {
		fmt.Println("hostNtos=fail")
		return
	}
	if strings.Contains(hb, `"isolation":"host"`) {
		fmt.Println("hostNtos=ok isolation=host")
	} else {
		fmt.Println("hostNtos=fail")
		return
	}

	cm, err := gocvm.Call("win32.occupyCmd", "")
	if err != nil {
		fmt.Println("cmd=fail")
		return
	}
	if strings.Contains(cm, `"permission":"sys"`) && strings.Contains(cm, "cmd.exe") && strings.Contains(cm, `"isolation":"shell"`) {
		fmt.Println("cmd=ok isolation=shell permission=sys")
	} else {
		fmt.Println("cmd=fail")
		return
	}

	lx, err := gocvm.Call("win32.occupyLinux", "")
	if err != nil {
		fmt.Println("linux=fail")
		return
	}
	if strings.Contains(lx, "wowwin32.ko") && strings.Contains(lx, `"isolation":"sys"`) {
		fmt.Println("linux=ok isolation=sys")
	} else {
		fmt.Println("linux=fail")
		return
	}

	wt, err := gocvm.Call("win32.occupyWasmtty", "")
	if err != nil {
		fmt.Println("wasmtty=fail")
		return
	}
	if strings.Contains(wt, `"wasmtty":true`) && strings.Contains(wt, `"isolation":"tty"`) && strings.Contains(wt, `"conpty":true`) && strings.Contains(wt, `"pts":true`) {
		fmt.Println("wasmtty=ok isolation=tty proto=WTTY")
	} else {
		fmt.Println("wasmtty=fail")
		return
	}

	gc, err := gocvm.Call("win32.occupyGocvm", "")
	if err != nil {
		fmt.Println("gocvm=fail")
		return
	}
	if strings.Contains(gc, `"gocvm":true`) && strings.Contains(gc, "wasigocvm") && strings.Contains(gc, `"permission":"sys"`) && strings.Contains(gc, `"toolkit":"gocvm"`) {
		fmt.Println("gocvm=ok image=wasigocvm isolation=tty permission=sys toolkit=gocvm")
	} else {
		fmt.Println("gocvm=fail")
		return
	}

	cl, err := gocvm.Call("win32.occupyCalc", "")
	if err != nil {
		fmt.Println("calc=fail")
		return
	}
	if strings.Contains(cl, `"toolkit":"gocvm"`) && strings.Contains(cl, "calc.exe") && strings.Contains(cl, `"win32":"CreateProcessW"`) && strings.Contains(cl, `"pid":`) {
		fmt.Println("calc=ok isolation=shell permission=sys image=calc.exe via=gocvm CreateProcessW")
	} else {
		fmt.Println("calc=fail")
		return
	}

	hi, err := gocvm.Call("win32.ttyHello", "")
	if err != nil {
		fmt.Println("ttyHello=fail")
		return
	}
	if strings.Contains(hi, `"magic":"WTTY"`) && strings.Contains(hi, `"role":1`) {
		fmt.Println("ttyHello=ok magic=WTTY role=1")
	} else {
		fmt.Println("ttyHello=fail")
		return
	}

	pg, err := gocvm.Call("win32.ttyPing", "")
	if err != nil {
		fmt.Println("ttyPing=fail")
		return
	}
	if strings.Contains(pg, "pong") {
		fmt.Println("ttyPing=ok pong")
	} else {
		fmt.Println("ttyPing=fail")
		return
	}

	nw, err := gocvm.Call("win32.occupyNet", "")
	if err != nil {
		fmt.Println("net=fail")
		return
	}
	if strings.Contains(nw, `"net":true`) && strings.Contains(nw, `"isolation":"net"`) && strings.Contains(nw, "sctp-rpc") {
		fmt.Println("net=ok isolation=net via=sctp-rpc")
	} else {
		fmt.Println("net=fail")
		return
	}
}
