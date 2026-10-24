// Encryptid iEDA. guikit compiles this onto an rmlui tree and main.go writes
// ieda.html. ieda.js boots HeapVM and sends the shell commands the engine
// already has: gpu fabric eda, hv, wsl. Edit this file, then regenerate.

style {
  :root { color-scheme: dark; }
  html, body { height: 100%; margin: 0; }
  body {
    background: radial-gradient(1100px 640px at 80% -10%, #1a140c 0%, #07090d 52%) fixed, #07090d;
    color: #e6e2d8;
    font-family: "Segoe UI", system-ui, -apple-system, sans-serif;
    font-size: 14px;
    overflow: auto;
  }
  header.top {
    display: flex; align-items: center; gap: 14px;
    height: 54px; padding: 0 22px; box-sizing: border-box;
    border-bottom: 1px solid #1a2230;
    background: rgba(8, 11, 16, 0.9);
  }
  span.logo { font-weight: 700; letter-spacing: 0.14em; color: #e8a87c; font-size: 13px; }
  span.title { font-weight: 650; font-size: 17px; }
  span.sub { color: #8b93a1; font-size: 12.5px; flex: 1; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  span.status { font-size: 12px; color: #8b93a1; padding: 5px 10px; border: 1px solid #253043; border-radius: 999px; white-space: nowrap; }
  span.status.ready { color: #e8a87c; border-color: #6b4a2a; }
  span.status.busy { color: #fbbf24; border-color: #5c4a1f; }
  span.status.fail { color: #f87171; border-color: #6b2525; }
  div.app { display: flex; min-height: calc(100% - 54px); }
  nav.side {
    width: 250px; flex: none; box-sizing: border-box; padding: 16px 12px;
    border-right: 1px solid #1a2230; background: rgba(9, 12, 18, 0.7);
    display: flex; flex-direction: column; gap: 4px;
  }
  div.navhead { color: #6b7280; font-size: 11px; letter-spacing: 0.14em; text-transform: uppercase; padding: 4px 10px 8px; }
  button.nav {
    display: flex; flex-direction: column; align-items: flex-start; gap: 2px; text-align: left;
    background: transparent; color: #cfd6e2; border: 1px solid transparent; border-radius: 10px;
    padding: 10px 12px; cursor: pointer; font: inherit;
  }
  button.nav:hover { background: #14110e; border-color: #2a241c; }
  button.nav.on { background: linear-gradient(90deg, rgba(232, 168, 124, 0.16), rgba(232, 168, 124, 0.03)); border-color: #6b4a2a; }
  span.navt { font-weight: 600; font-size: 14px; }
  span.navs { color: #7c8594; font-size: 12px; }
  div.navfoot { margin-top: auto; padding: 12px 10px 4px; color: #5b6472; font-size: 11.5px; line-height: 1.5; }
  main.stage { flex: 1; min-width: 0; padding: 22px 26px 48px; box-sizing: border-box; }
  section.view { display: none; }
  section.view.on { display: block; }
  h1 { margin: 0 0 6px; font-size: 24px; font-weight: 650; letter-spacing: -0.01em; }
  p.blurb { margin: 0 0 16px; color: #9aa3b2; max-width: 920px; line-height: 1.55; }
  div.controls {
    display: flex; flex-wrap: wrap; align-items: flex-end; gap: 12px;
    padding: 14px 16px; margin-bottom: 16px;
    background: rgba(13, 17, 23, 0.85); border: 1px solid #2a241c; border-radius: 12px;
  }
  label.ctl { display: flex; flex-direction: column; gap: 6px; color: #8b93a1; font-size: 11.5px; letter-spacing: 0.06em; text-transform: uppercase; }
  select, input.path {
    background: #0a0e14; color: #e6e2d8; border: 1px solid #3a3228; border-radius: 8px;
    padding: 8px 10px; font: inherit; font-size: 13.5px; min-width: 140px;
  }
  input.path { min-width: 260px; flex: 1; }
  button.run, button.quiet {
    padding: 10px 16px; border-radius: 10px; cursor: pointer; font: inherit; font-weight: 650;
  }
  button.run { border: 1px solid #6b4a2a; background: linear-gradient(180deg, #6a4328, #3c2616); color: #ffe8d6; }
  button.quiet { border: 1px solid #263042; background: #10151d; color: #d5dbe6; font-weight: 600; }
  button.run:hover, button.quiet:hover { filter: brightness(1.12); }
  button.run:disabled, button.quiet:disabled { opacity: 0.5; cursor: wait; }
  div.work { display: grid; grid-template-columns: 300px minmax(0, 1fr); gap: 16px; align-items: start; }
  aside.lib, div.card {
    background: rgba(13, 17, 23, 0.85); border: 1px solid #1d2430; border-radius: 12px; overflow: hidden;
  }
  div.cardh, div.libh { padding: 10px 14px; color: #aab3c2; font-size: 12.5px; border-bottom: 1px solid #161d28; }
  div.list { max-height: 520px; overflow: auto; }
  button.design {
    display: block; width: 100%; text-align: left; background: transparent; color: #d5dbe6;
    border: 0; border-bottom: 1px solid #141b25; padding: 10px 14px; cursor: pointer; font: inherit;
  }
  button.design:hover { background: #121820; }
  button.design.on { background: rgba(232, 168, 124, 0.1); }
  span.dname { display: block; font-weight: 650; }
  span.dmeta { display: block; color: #7c8594; font-size: 12px; margin-top: 2px; }
  table.facts { width: 100%; border-collapse: collapse; font-size: 13px; }
  table.facts td { padding: 7px 14px; border-bottom: 1px solid #141b25; color: #c7cfdb; }
  table.facts td.k { color: #7c8594; width: 34%; }
  pre.out {
    margin: 0; padding: 12px 14px; max-height: 420px; overflow: auto;
    white-space: pre-wrap; word-break: break-word;
    font-family: Consolas, "Cascadia Mono", ui-monospace, monospace;
    font-size: 12.5px; line-height: 1.45; color: #d5dbe6;
  }
  div.vgpu { background: #05070a; }
  canvas#vgpu { display: block; width: 100%; height: 220px; background: #05070a; }
  div.grid { display: grid; gap: 16px; grid-template-columns: repeat(12, 1fr); }
  div.card.w12 { grid-column: span 12; }
  div.card.w8 { grid-column: span 8; }
  div.card.w6 { grid-column: span 6; }
  div.card.w4 { grid-column: span 4; }
  div.cardh { display: flex; justify-content: space-between; gap: 10px; }
  span.note { color: #6b7280; }
  div.plot { position: relative; height: 300px; background: #07090d; }
  div.plot.h420 { height: 420px; }
  div.plot canvas { position: absolute; inset: 0; width: 100%; height: 100%; display: block; }
  div.plot canvas.ovl { pointer-events: none; }
  div.layout { display: flex; align-items: center; justify-content: center; background: #05070a; height: 380px; }
  div.layout img { max-width: 100%; max-height: 100%; }
  p#si-err, p#time-err { color: #f87171; }
  table.paths { width: 100%; border-collapse: collapse; font-size: 12.5px; }
  table.paths th, table.paths td { padding: 6px 10px; border-bottom: 1px solid #141b25; text-align: left; }
  table.paths th { color: #7c8594; font-weight: 600; position: sticky; top: 0; background: #10151c; }
  table.paths tr { cursor: pointer; }
  table.paths tr.on { background: rgba(232, 168, 124, 0.12); }
  table.paths td.met { color: #5eead4; }
  table.paths td.viol { color: #f87171; }
  table.paths td.pin { max-width: 280px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  div.paths { max-height: 460px; overflow: auto; }
  @media (max-width: 1100px) {
    div.card.w8, div.card.w6, div.card.w4, div.card.w12 { grid-column: span 12; }
  }
  @media (max-width: 860px) {
    header.top { height: auto; flex-wrap: wrap; padding: 10px 14px; gap: 8px; }
    span.sub { flex: 1 1 100%; white-space: normal; }
    div.app { flex-direction: column; }
    nav.side { width: auto; flex-direction: row; flex-wrap: wrap; border-right: 0; border-bottom: 1px solid #1a2230; }
    div.navfoot { display: none; }
    main.stage { padding: 16px 14px 32px; }
    div.work { grid-template-columns: 1fr; }
    input.path { min-width: 0; width: 100%; }
    button.run { margin-left: 0; }
  }
}

view {
  header.top (
    span.logo ("ENCRYPTID")
    span.title ("iEDA")
    span.sub ("Post-SIMD Harvard MAC designs on Encryptid MicroVM. Emit and check stay in HeapVM. Timing paths are the signed-off reports.")
    span#status.status ("starting the engine")
  )
  div.app (
    nav.side (
      div.navhead ("Encryptid Systems")
      button#nav-vm.nav.on :data-view."vm" (
        span.navt ("Encryptid MicroVM")
        span.navs ("WASM hypervisor, WSL")
      )
      button#nav-eda.nav :data-view."eda" (
        span.navt ("iEDA")
        span.navs ("Harvard MAC")
      )
      button#nav-si.nav :data-view."si" (
        span.navt ("Silicon")
        span.navs ("SKY130 signoff")
      )
      button#nav-time.nav :data-view."time" (
        span.navt ("Timing")
        span.navs ("OpenSTA paths")
      )
      div.navfoot ("Both panes type shell commands into HeapVM. The synthesizer stays in the engine.")
    )
    main.stage (
      section#view-vm.view.on (
        h1 ("Encryptid MicroVM")
        p.blurb ("A hypervisor in WASM. WSL boots a distro on it: the kernel at C:\\WSL\\kernel, an ext2 root on a virtio-scsi disk, and init at /sbin/wsl-init. The guest treats that machine as its hypervisor and reads the CPUID it answers. These buttons send hv, wsl --status, wsl -l, and wsl-alpine. Boot Alpine takes the console; the guest stays there until it stops.")
        div.controls (
          button#hv-run.run :type."button" ("Read CPUID")
          button#wsl-status.quiet :type."button" ("WSL status")
          button#wsl-list.quiet :type."button" ("List distros")
          button#wsl-boot.quiet :type."button" ("Boot Alpine")
        )
        div.card (
          div.cardh ("What the guest reads")
          pre#vm-out.out ("The engine has not answered yet.")
        )
      )
      section#view-eda.view (
        h1 ("iEDA")
        p.blurb ("Electronic design automation for a Harvard MAC. The instruction side is the spatial netlist and the Jev microcode ROM, with no program counter. The data side is separate: streams, weight registers, SRAM, Wishbone, and SPI. The word path is a multiply-accumulate of 32-bit processing elements beside LUT4 control. Emit, check, and import go through gpu fabric eda. Files are written to C:\\eda. The console reply stops at 16 KB; download C:\\eda saves the rest as eda.zip.")
        div.work (
          aside.lib (
            div.libh ("Designs from gpu fabric eda list")
            div#designs.list ()
          )
          div.col (
            div.controls (
              button#eda-refresh.quiet :type."button" ("Refresh library")
              label.ctl ("Format" select#fmt (
                option :value."verilog" ("Verilog")
                option :value."vhdl" ("VHDL")
                option :value."blif" ("BLIF")
                option :value."tb" ("Verilog testbench")
                option :value."vtb" ("VHDL testbench")
                option :value."rom" ("Jev ROM")
                option :value."qasm" ("OpenQASM")
                option :value."all" :selected."true" ("Every format")
              ))
              button#emit.run :type."button" ("Emit")
              button#check.quiet :type."button" ("Check")
              button#download.quiet :type."button" ("Download C:\\eda")
            )
            div.card (
              div.cardh ("Selected design")
              table.facts (
                tr (td.k ("Name") td#f-name ("—"))
                tr (td.k ("LUT") td#f-lut ("—"))
                tr (td.k ("Word PE") td#f-pe ("—"))
                tr (td.k ("Registers") td#f-reg ("—"))
                tr (td.k ("Jev ROMs") td#f-rom ("—"))
                tr (td.k ("Emitters") td#f-emit ("—"))
                tr (td.k ("What it is") td#f-what ("—"))
              )
            )
            div.controls (
              label.ctl ("Guest BLIF path" input#blif-path.path :type."text" :value."C:\\eda\\cells.blif")
              button#import.quiet :type."button" ("Import BLIF")
            )
            div.card (
              div.cardh ("Engine reply")
              pre#eda-out.out ("Library loads after the engine boots.")
            )
            div.card (
              div.cardh ("Emitted text, via type")
              pre#eda-src.out ("")
            )
            div.card (
              div.cardh ("vGPU")
              div.vgpu (canvas#vgpu ())
            )
          )
        )
      )
      section#view-si.view (
        h1#si-heading ("Silicon")
        p#si-blurb.blurb ("Both chips hardened on SKY130. Layouts, slack, power, and clock skew come from the signoff reports.")
        p#si-err ()
        div.grid (
          div.card.w6 ( div.cardh ( span#si-card-pipe ("jev_dot_pipe") ) div.layout ( img#si-pipe-img () ) )
          div.card.w6 ( div.cardh ( span#si-card-spi ("jev_dot_spi") ) div.layout ( img#si-spi-img () ) )
          div.card.w8 (
            div.cardh ( span#si-card-slack ("Setup slack by corner") span#si-card-slack-note.note ("teal is the first chip, violet is the second") )
            div.plot.h420 ( canvas#si-slack.gpu () )
          )
          div.card.w4 ( div.cardh ( span#si-card-facts ("Signoff") ) table#si-facts.facts () )
          div.card.w6 (
            div.cardh ( span#si-card-power ("Simulated power") span#si-card-power-note.note ("activity on the routed netlist, mW") )
            div.plot.h420 ( canvas#si-power.gpu () )
          )
          div.card.w6 (
            div.cardh ( span#si-card-skew ("Clock skew") span#si-card-skew-note.note ("worst setup skew, ns") )
            div.plot.h420 ( canvas#si-skew.gpu () )
          )
        )
      )
      section#view-time.view (
        h1#time-heading ("Timing")
        p#time-blurb.blurb ("Worst paths from the signed-off OpenSTA reports. iEDA reads those reports for each chip and corner. The static timing run stays the one that signed the chip off.")
        p#time-err ()
        div.controls (
          label.ctl ("Chip" select#time-chip ())
          label.ctl ("Corner" select#time-corner ())
          label.ctl ("Check" select#time-kind (
            option :value."max" :selected."true" ("Setup max")
            option :value."min" ("Hold min")
          ))
          button#time-load.run :type."button" ("Load paths")
        )
        div.grid (
          div.card.w12 ( div.cardh ( span#time-card-sum ("This report") ) table#time-sum.facts () )
          div.card.w12 (
            div.cardh ( span#time-card-paths ("Paths") span#time-card-note.note ("the first paths in the report, worst first") )
            div.paths ( table.paths (
              thead ( tr ( th ("#") th ("Startpoint") th ("Endpoint") th ("Slack ns") th ("Arrival") th ("Required") ) )
              tbody#time-rows ()
            ) )
          )
          div.card.w12 (
            div.cardh ( span#time-card-detail ("Selected path") )
            pre#time-detail.out ("Select a path.")
          )
        )
      )
    )
  )
}
