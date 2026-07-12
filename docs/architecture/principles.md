# AAMP Architectural Principles

## Purpose

This document defines the architectural principles that govern the AAMP
project.

These principles are intentionally stable and should outlive
implementation details. Any architectural change must be evaluated
against them before being adopted.

------------------------------------------------------------------------

## Principle 1 --- Clear Separation of Responsibilities

The AAMP is composed of two independent engines:

-   **Acquisition Engine**
-   **Laboratory Engine**

Each engine has a single, well-defined responsibility.

------------------------------------------------------------------------

## Principle 2 --- Acquisition Produces Evidence

The Acquisition Engine is responsible only for interacting with hardware
and producing reproducible evidence.

Its responsibilities include:

-   Hardware discovery
-   Device configuration
-   Stimulus generation
-   Audio playback
-   Audio capture
-   Session synchronization
-   Evidence Package generation

The Acquisition Engine **must not** interpret measurements.

------------------------------------------------------------------------

## Principle 3 --- Laboratory Produces Knowledge

The Laboratory Engine is responsible for every scientific
interpretation.

This includes:

-   Loading Evidence Packages
-   Signal processing
-   FFT and spectral analysis
-   Time-domain analysis
-   Distortion analysis
-   Graph generation
-   Investigation engine
-   Report generation

Every scientific result presented to the user originates from the
Laboratory Engine.

------------------------------------------------------------------------

## Principle 4 --- The Evidence Package Is the Only Contract

Communication between the engines occurs exclusively through the `.aamp`
Evidence Package.

Neither engine should depend on the internal implementation of the
other.

------------------------------------------------------------------------

## Principle 5 --- Preserve the User Experience

Architectural evolution must not alter the user workflow unnecessarily.

Changes to the internal implementation should preserve:

-   Interface behavior
-   Navigation
-   Measurement workflow
-   Investigation workflow

------------------------------------------------------------------------

## Principle 6 --- Reproducibility

Every characterization must be reproducible from the Evidence Package
alone.

A future Laboratory Engine must be capable of reopening an old `.aamp`
file and reproducing the same analyses.

------------------------------------------------------------------------

## Principle 7 --- Platform Independence

The Laboratory Engine should remain independent of operating-system
APIs.

Only the Acquisition Engine is platform-specific.

This architecture enables future support for additional operating
systems by replacing only the Acquisition Engine.

------------------------------------------------------------------------

## Principle 8 --- Scientific Transparency

Every displayed metric must be traceable to the captured evidence.

Scientific calculations should remain deterministic and independently
verifiable.

------------------------------------------------------------------------

## Principle 9 --- Documentation Before Complexity

Whenever a new architectural concept is introduced:

-   document it;
-   justify it;
-   preserve the rationale.

Architecture should never rely on tribal knowledge.

------------------------------------------------------------------------

## Principle 10 --- Simplicity

Prefer simpler architectures when they satisfy the same scientific and
technical requirements.

Avoid unnecessary layers, abstractions, or duplicated responsibilities.

------------------------------------------------------------------------

These principles define the long-term architectural identity of the AAMP
project and should guide future development regardless of implementation
language or technology.
