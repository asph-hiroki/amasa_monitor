# AGENTS.md

## Project

This project is `amasa_monitor`, a CERN ROOT-based experimental data monitoring system.

The system may need to read `.root` files, inspect TTrees, extract histograms or metrics, and generate monitoring reports or dashboards.

## Important context

- The main research environment is a Linux laboratory server.
- The user also sometimes works on Windows 11 with WSL2 Ubuntu.
- The project should avoid committing real experimental data.
- Small synthetic or anonymized sample ROOT files may be placed under `data/sample/`.

## ROOT-related rules

- Before implementing data loading, inspect `docs/spec.md` and `docs/root_data_format.md`.
- Do not assume all input data is CSV.
- Consider whether PyROOT, uproot, or C++ ROOT is appropriate.
- Prefer isolating ROOT-dependent code under `src/amasa_monitor/io/`.
- If ROOT is unavailable in the development environment, provide a fallback test strategy using mocks or small sample data.
- Do not commit large `.root` files or private experimental data.

## Coding rules

- Work in small, reviewable steps.
- Add tests for implemented features.
- Keep setup instructions clear for both the lab Linux server and WSL2 Ubuntu.
- Do not commit secrets, API keys, passwords, or private files.

## Initial task style

For the first task, do not write code.
Create an implementation plan first.
