# Migration Plan

## Purpose

This document defines the official migration strategy for the AAMP.

It is **not** an implementation guide.

It defines the architectural destination of the project.

Every implementation decision must converge to the architecture described here.

---

# Scope

This migration reorganizes the internal responsibilities of the AAMP.

It does not define a new product.

It does not define a new user interface.

It does not change the measurement workflow.

It does not change the user experience.

It does not introduce new scientific concepts.

Its sole purpose is to establish a clean separation between hardware acquisition and scientific analysis.

---

# Fundamental Principle

The project shall be divided into two independent engines.

## Acquisition Engine

Responsible for producing measurement evidence.

## Laboratory Engine

Responsible for interpreting measurement evidence.

The Acquisition Engine never produces scientific results.

The Laboratory Engine never communicates directly with hardware.

---

# User Experience Preservation

The user shall not perceive architectural changes.

The following shall remain unchanged:

- application layout
- navigation
- workflow
- dashboard
- investigation
- charts
- reports
- language support
- project organization
- WebView integration

The migration is strictly internal.

---

# Acquisition Engine Responsibilities

The Acquisition Engine shall become exclusively responsible for:

- hardware discovery
- hardware configuration
- playback
- capture
- synchronization
- latency compensation
- stimulus generation
- waveform generation
- WAV recording
- Evidence Package generation
- progress reporting

No scientific analysis shall exist inside the Acquisition Engine.

---

# Scientific Responsibility

The Acquisition Engine shall never calculate:

- FFT
- Magnitude response
- Phase response
- Group Delay
- Excess Group Delay
- Impulse Response
- Waterfall
- Spectrogram
- THD
- THD+N
- IMD
- Dynamic Range
- Crosstalk
- Noise analysis
- Investigation
- Dashboard metrics
- Report metrics

Those responsibilities belong exclusively to the Laboratory Engine.

---

# Laboratory Engine

The Laboratory Engine becomes the scientific core of the AAMP.

Its responsibilities include:

- opening Evidence Packages
- reading captured WAV files
- loading stimulus WAV files
- executing all signal processing
- producing every metric
- generating every graph
- executing Investigation
- exporting reports

Every metric shown to the user shall originate from the Laboratory Engine.

---

# Evidence Package

The communication contract between both engines becomes the Evidence Package.

No ResultBundle shall remain after the migration.

No scientific object shall be exchanged between backend and frontend.

The Evidence Package becomes the single source of truth.

---

# Evidence Package Content

Each characterization shall generate a single .aamp file.

The package shall contain every artifact required to reproduce the complete characterization.

Minimum contents:

- session metadata
- hardware metadata
- acquisition metadata
- stimulus WAV files
- captured WAV files

Future versions may include:

- reports
- cache
- photographs
- firmware
- presets
- comments

The architecture shall support expansion without changing the contract.

---

# Stimulus Recording

Every executed stimulus shall generate:

STxx_Source.wav

and

STxx_Capture.wav

No stimulus may exist without its recorded evidence.

---

# Source of Truth

After this migration, scientific results shall never become the source of truth.

The captured audio becomes the source of truth.

Scientific results become reproducible products generated from the evidence.

---

# Bridge

The bridge shall become an instrumentation interface.

Examples of responsibilities:

- enumerate hardware
- configure hardware
- start characterization
- cancel characterization
- progress reporting
- session creation
- Evidence Package delivery

The bridge shall never transmit scientific metrics.

---

# WebView

The WebView remains the official application interface.

No replacement shall occur.

No redesign shall occur.

The migration shall preserve the complete user interface.

Only the internal data source changes.

---

# Existing Infrastructure

The following components shall remain part of the project:

- JUCE
- WebView2
- Hardware Manager
- Routing Manager
- Device Manager
- Capture Engine
- Playback
- Session management

Their responsibilities may change, but their existence is preserved whenever appropriate.

---

# Analysis Engine

The current Analysis Engine shall cease to be the scientific core.

Its scientific responsibilities shall migrate to the Laboratory Engine.

Remaining backend code shall support acquisition only.

---

# Validation Philosophy

Scientific validation is intentionally postponed.

This migration focuses exclusively on architecture.

Metric validation will occur after the migration using independent references, including but not limited to:

- Python
- REW
- Ozone
- professional laboratory tools

No architectural decision shall be blocked by temporary metric differences.

---

# Legacy Components

Legacy components shall not constrain the migration.

Whenever a component conflicts with the new architecture, the architecture takes precedence.

Compatibility with obsolete internal designs is not required.

---

# Code Preservation

Existing code shall be reused whenever possible.

The objective is refactoring, not rewriting.

Well-designed components shall be preserved.

Only responsibilities shall change.

---

# Documentation Authority

Implementation decisions shall follow the following authority order.

1. docs/architecture/
2. active project source code
3. historical documentation (archive) only for historical consultation

Archive documents shall never override the official architecture.

---

# Forbidden Decisions

The following architectural decisions are explicitly forbidden.

- placing scientific analysis inside the Acquisition Engine
- calculating scientific metrics during acquisition
- creating duplicate scientific implementations
- creating temporary architectures
- introducing compatibility layers without justification
- changing the user interface
- changing the measurement workflow
- changing the Investigation workflow

---

# Target Architecture

The final architecture shall follow the structure below.

Hardware

↓

Acquisition Engine

↓

Evidence Package (.aamp)

↓

Laboratory Engine

↓

Scientific Analysis

↓

Investigation

↓

Reports

---

# Completion Criteria

The migration shall be considered complete when:

- Acquisition Engine produces only evidence;
- Laboratory Engine produces every scientific result;
- the .aamp package becomes the only contract;
- the user interface remains unchanged;
- the application workflow remains unchanged;
- the architecture follows the principles defined in docs/architecture.

# Implementation Autonomy

The implementation may reorganize classes, files, modules and internal structures whenever necessary.

However, no implementation decision may violate the architectural principles defined in:

- principles.md
- architecture.md
- evidence-package.md

Whenever implementation convenience conflicts with architecture, the architecture shall prevail.
