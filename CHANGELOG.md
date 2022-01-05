# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [Unreleased]

## [0.1.0] - 2022-01-05
### Added
- Spatz top level module
- Controller that includes:
    - An instruction decoder
    - A scoreboard
    - Vector CSR registers
    - An operation issuer
- Vector Load Store Unit (VLSU) with support for:
    - Unit-stride load and stores (`vle8.v`, `vle16.v`, `vle32.v`, `vse8.v`, `vse16.v`, `vse32.v`)
    - Strided load and stores (`vlse8.v`, `vlse16.v`, `vlse32.v`, `vsse8.v`, `vsse16.v`, `vsse32.v`)
    - Configurable number of memory ports and Spatz IPUs.
- Vector Functional Unit (VFU) with support for:
    - Single-width integer add and subtract (`vadd.{vv, vx, vi}`, `vsub.{vv, vx, vi}`)
    - Bitwise logical (`vand.{vv, vx, vi}`, `vor.{vv, vx, vi}`, `vxor.{vv, vx, vi}`)
    - Single-width shift (`vsll.{vv, vx, vi}`, `vsrl.{vv, vx, vi}`, `vsra.{vv, vx, vi}`)
    - Single-width integer multiply (`vmul.{vv, vx}`, `vmulh.{vv, vx}`, `vmulhu.{vv, vx}`, `vmulhsu{vv, vx}`)
    - Single-width integer multiply-add (`vmacc.{vv, vx}`, `vnmsac.{vv, vx}`, `vmadd.{vv, vx}`, `vnmsub.{vv, vx}`)
    - Integer move (`vmv.v.{v, x, i}`)
- Vector Register File (VRF) to temporarily store vectors
- RSIC-V vector tests for all vector instructions
- Basic stipmining test application
- Vector matmul application
