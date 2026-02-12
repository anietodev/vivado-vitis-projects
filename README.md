# vivado-vitis-projects

Repository that organizes projects for Vivado (FPGA hardware design) and Vitis (heterogeneous applications on Zynq, Vitis platform, etc.) without uploading workspaces or generated files.  
**Only the files necessary to rebuild the projects from scratch are included.**

---

## Structure

```
vivado-vitis-projects/
├── docs/                # General documentation
├── vitis/               # Vitis projects (applications, platforms, etc.)
├── vivado/              # Vivado projects (HDL, constraints, scripts, IP)
```

## How to rebuild a project

### Vivado

From the root folder:

```sh
cd vivado/ejemplo_vivado/scripts
vivado -mode batch -source create_project.tcl
```

This generates the Vivado workspace and automatically adds the sources, constraints, and any IP/devices listed in the script.

### Vitis

Read the README inside each subfolder of `vitis/` for instructions on building/importing the project, either by command or by GUI.

---
