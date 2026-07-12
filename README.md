# AAMP

Professional Audio Analysis and Measurement Platform

AAMP is architecturally divided into two independent engines:

- **Acquisition Engine** — responsible for hardware communication, stimulus playback, audio capture and Evidence Package generation.
- **Laboratory Engine** — responsible for all scientific analysis, visualization, investigation and reporting.

The two engines communicate exclusively through the **Evidence Package (`.aamp`)**, which is the official architectural contract of the system.

---

## Documentation

### Official Architecture

The official architecture is defined exclusively in:

```
docs/architecture
```

### Scientific Validation

Validation procedures and scientific verification documents:

```
docs/validation
```

### Laboratory Documentation

Laboratory-specific documentation:

```
docs/laboratory
```

### Historical Documentation

Historical documents from previous project phases:

```
docs/archive
```

These documents are maintained for historical reference only and **must not** be used as the architectural authority for future development.

---

For implementation details, architectural principles and migration strategy, always refer to the documentation under `docs/architecture`.