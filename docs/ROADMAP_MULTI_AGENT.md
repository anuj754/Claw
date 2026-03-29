# TizenClaw Multi-Agent & Perception Roadmap

> **Date**: 2026-03-11
> **Reference**: [Project Analysis](ANALYSIS.md) | [System Design](DESIGN.md)

---

## 1. Overview
As TizenClaw matures to handle complex, long-running operational workflows within constrained embedded environments, the monolithic session-based agent approach is transitioning toward a highly decentralized, robust **11 MVP Agent Set** supported by an advanced **Perception Layer**.

This roadmap details the transition plan.

---

## 2. Phase A: Formulate the MVP Agent Set

### The 11-Agent MVP Set
To achieve operational stability on embedded devices, the existing Orchestrator and Skill Manager will be fractured and specialized into 11 distinct responsibilities across 7 categories.

| Category | Agent | Primary Responsibility |
|----------|-------|------------------------|
| **Understanding** | `Input Understanding Agent` | Standardizes user input across all channels into a unified intent structure. |
| **Perception** | `Environment Perception Agent` | Subscribes to the Event Bus to maintain the Common State Schema. |
| **Memory** | `Session / Context Agent` | Manages working memory (current task), long-term memory (user preferences), and episodic memory. |
| **Planning** | `Planning Agent` (Orchestrator) | Decomposes goals into logical steps based on the Capability Registry. |
| **Execution** | `Action Execution Agent` | Invokes the actual OCI Container Skills and Action Framework commands. |
| **Protection** | `Policy / Safety Agent` | Intercepts plans prior to execution to enforce restrictions (e.g. constraints). |
| **Utility** | `Knowledge Retrieval Agent` | Interfaces with the SQLite RAG store for semantic lookups. |
| **Monitoring** | `Health Monitoring Agent` | Monitors memory pressure (PSS constraints), daemon uptime, and container health. |
| | `Recovery Agent` | Analyzes structured failures (e.g. DNS timeout) and attempts fallback logic or error correction. |
| | `Logging / Trace Agent` | Centralizes context for debugging and audit logs. |

*(The legacy `Skill Manager` agent will be phased out or absorbed into the Execution/Recovery layers as RPK-based tool delivery matures.)*

---

## 3. Phase B: Implement the Perception Architecture

A robust multi-agent system relies on high-quality perception. TizenClaw's perception layer is designed around the following pillars:

### 3.1 Common State Schema
Normalize raw `/proc` data or disjointed logs into continuous JSON schemas:
- `DeviceState`: Active capabilities (Display, BT, WiFi), Model, Name.
- `RuntimeState`: Network status, memory pressure, power mode.
- `UserState`: Locale, preferences, role.
- `TaskState`: Current goal, active step, missing intent slots.

### 3.2 Capability Registry & Function Contracts
All dynamic RPK plugins, CLI tools, and built-in skills must register against a structured Capability Registry with a clear Function Contract (Input/Output Schemas, Side Effects, Retry Policies, Required Permissions).

### 3.3 Event Bus (Event-Driven Updates)
The system will react to granular events (e.g. `sensor.changed`, `network.disconnected`, `user.command.received`, `action.failed`) to maintain state freshness without CPU taxation.

### 3.4 Isolated Memory Structures
- *Short-term*: Current dialog, recent commands, immediate fail reasons.
- *Long-term*: User preferences, typical usage profiles.
- *Episodic*: Historical records of which skill executions succeeded/failed under specific conditions.

### 3.5 Embedded Design Principles
- **Selective Context Injection**: Only provide the necessary state to the LLM. interpreted state rather than raw data—e.g., `[network: disconnected, reason: dns_timeout]` is better than 1,000 lines of `dlog`.
- **Separation of Perception and Execution**: The Perception Agent reads the state, the Execution Agent alters it.
- **Confidence Scoring**: Intent and Object detection yield confidence scores (e.g. `confidence: 0.82`), permitting the system to ask clarifying questions when certainty is low.

---

## 4. Phase C: Extensibility via RPK Tool Distribution

With the shift toward structured capabilities and function contracts, the final phase introduces **RPK Tool Distribution**.

Tizen Resource Packages (RPKs) will bundle:
- Sandboxed Python Skills
- Host/Container CLI-based tools

These packages will dynamically populate the `Capability Registry` without requiring daemon recompilation.
