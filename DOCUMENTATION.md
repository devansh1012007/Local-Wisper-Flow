# Project Documentation

## 1. Overview

This repository implements a headless dictation engine foundation with a local web dashboard. The codebase is intentionally modular so that the core runtime, state handling, persistence, and UI can evolve independently.

At a high level, the project is organized around four responsibilities:

- Runtime orchestration: managing sessions, state transitions, and events.
- Audio and speech pipeline scaffolding: a simple event-driven backbone for future integrations with real speech-to-text, LLM, and text-to-speech services.
- Persistence: storing session and history data in JSON files so the engine survives restarts.
- Local UI: exposing the runtime through a small browser-based control center.

This project is best understood as a foundation for a real dictation engine rather than a complete production-ready speech system.

---

## 2. Repository Structure

The repository root contains:

- Dockerfile: container image definition for running the engine and UI.
- docker-compose.yml: local multi-container setup for the engine and UI.
- pyproject.toml: package metadata, dependencies, and tooling configuration.
- README.md: short usage instructions.
- proto/: protobuf definitions for gRPC.
- src/vision_sst/: primary Python package.
- tests/: unit tests covering the engine, UI, and container scaffolding.

### Source package layout

- src/vision_sst/__init__.py: package marker.
- src/vision_sst/__main__.py: CLI entrypoint for running the gRPC engine.
- src/vision_sst/events.py: event model definitions.
- src/vision_sst/state_machine.py: explicit state machine for dictation workflow.
- src/vision_sst/services/audio.py: minimal audio service scaffold.
- src/vision_sst/plugins/: abstractions for SST, TTS, and LLM integrations.
- src/vision_sst/engine/: engine runtime, persistence, and gRPC server/client.
- src/vision_sst/ui.py: dashboard HTML generation.
- src/vision_sst/ui_server.py: lightweight HTTP server serving the dashboard and APIs.

---

## 3. Core Design Principles

### 3.1 Event-driven architecture
The system is organized around events rather than direct imperative calls. Components emit typed events, and the engine service can collect and expose them. This makes it easier to add new stages without tightly coupling components.

### 3.2 Explicit finite state machine
The dictation flow is modeled as a finite state machine to make transitions easy to reason about. States represent phases such as idle, armed, capturing, transcribing, post-processing, and emitting.

### 3.3 Layered separation
The codebase isolates:

- domain model (events and FSM)
- service layer (session lifecycle and runtime state)
- transport layer (gRPC and UI server)
- persistence layer (JSON stores)

### 3.4 Pluggable backends
The plugin interfaces define what an SST, TTS, or LLM backend must implement. The registry is designed to discover and register plugin modules dynamically.

---

## 4. Runtime Concepts

### 4.1 Session
A session represents a single dictation interaction. The SessionRecord dataclass stores:

- id: unique session UUID
- mode: runtime mode such as toggle
- language: language hint
- started_at: start timestamp
- status: current lifecycle status
- preview: optional text preview

### 4.2 Event
Events are immutable dataclasses created with a type, payload, timestamp, session id, sequence, and source. The event model is defined in events.py.

### 4.3 State machine
The DictationFSM tracks the current phase of the workflow. The transitions encode the expected flow from idle to armed to capture/transcribe/process/emit, with abort and failure branches.

---

## 5. Module-by-Module Explanation

## 5.1 src/vision_sst/events.py

### Purpose
This module defines the event contract used throughout the engine.

### Important contents
- EventType: a literal union of supported event names.
- Event dataclass: frozen, immutable object representing an event instance.
- Event.create(): convenience constructor that produces a timestamped event with a generated session id when one is not supplied.

### Why it matters
Events are the universal language of the system. The UI, engine service, and future integrations should all work in terms of these event objects rather than ad hoc dictionaries.

### Notes for developers
- Extend EventType carefully when adding new pipeline stages.
- Keep payloads simple and serializable for UI or transport consumption.

---

## 5.2 src/vision_sst/state_machine.py

### Purpose
This module implements the explicit finite state machine used to model the dictation lifecycle.

### States
- IDLE: no active capture flow
- ARMED: ready to listen
- CAPTURING: audio is being collected
- TRANSCRIBING: speech audio is being converted to text
- POSTPROCESSING: the transcription is being refined or enriched
- EMITTING: the final output is being delivered

### Triggers
The FSM handles triggers such as hotkey press/release, speech/silence, transcription completion/failure, LLM completion, timeout, and abort.

### Transition definition
The TRANSITIONS list contains explicit state/trigger pairs. Each transition may define:

- from_state
- trigger
- to_state
- guard
- action

### Runtime behavior
The trigger() method looks for a valid transition matching the current state and trigger. If a guard blocks it, nothing happens. Otherwise, it updates the state, executes the action if present, and notifies the callback.

### Why it matters
This is the control logic that makes the system deterministic. A real implementation can hook this FSM into audio/VAD/SST/LLM/TTS layers without changing the transport or persistence behavior.

---

## 5.3 src/vision_sst/services/audio.py

### Purpose
A minimal Phase 0 audio service scaffold.

### Responsibilities
- Starting and stopping capture state
- Processing audio chunks
- Emitting audio events

### Implementation details
- AudioService maintains a simple _running flag.
- start() sets the running flag and emits an event indicating capture started.
- stop() stops capture and emits a capture stopped event.
- process_chunk() computes a simple RMS-based amplitude value and converts it to a decibel-like value before emitting an audio:level event.

### Why it matters
This is not a real microphone integration. It demonstrates the event flow that a production audio backend would eventually plug into.

### Limitations
- It does not access the OS audio subsystem.
- It does not perform real VAD or streaming audio capture.
- It is deterministic and simple for tests.

---

## 5.4 src/vision_sst/plugins/base.py

### Purpose
Defines the abstract interfaces for pluggable backends.

### Core abstractions
- Capability: metadata describing a plugin implementation
- TranscriptionResult: structured result from an SST plugin
- SSTPlugin: interface for speech-to-text backends
- TTSPlugin: interface for text-to-speech backends
- LLMPlugin: interface for language-model post-processing backends

### Why it matters
These interfaces form the extension boundary for the engine. A future implementation can add a real Whisper, OpenAI, or Azure plugin by implementing the appropriate interface and registering it.

---

## 5.5 src/vision_sst/plugins/registry.py

### Purpose
Provides a lightweight plugin discovery and registration mechanism.

### Runtime behavior
- The registry holds three dictionaries: SST, TTS, and LLM plugins.
- register_sst(), register_tts(), and register_llm() attach implementations by name.
- get_sst() retrieves a plugin by name or raises KeyError.
- list_sst() returns capability metadata.
- discover() scans a list of directories for Python modules and imports any module exposing a register() function.

### Why it matters
This makes the engine ready for future plugin loading without hardcoding concrete backends.

---

## 5.6 src/vision_sst/engine/service.py

### Purpose
This is the central engine service that owns the runtime lifecycle for sessions, events, and status.

### Important state
- self.fsm: the state machine instance
- self._sessions: active/in-memory session records
- self._events: recent event history
- self._audio_level: latest audio metric
- self._model_loaded: placeholder model readiness flag
- self._store and self._history_store: persistence backends

### Key methods
- __init__(): creates the FSM, lock, stores, and loads persisted data.
- _load_from_store(): hydrates session records from the session store.
- start_session(): creates a new session, writes it to store/history, and records a system event.
- stop_session(): marks a session as stopped and persists the update.
- get_status(): returns a lightweight status dictionary with state, model_loaded, and audio_level.
- add_event(): appends events to the internal event list and updates audio level when appropriate.
- get_events(): returns the most recent events.
- get_history(): returns a list of recent history items sorted by recency.
- process_transition(): converts string triggers to enum values and sends them to the FSM.

### Why it matters
This is the core runtime brain of the project. All higher-level components depend on it, directly or indirectly.

### Concurrency note
The service uses a threading.Lock to protect shared state, which is appropriate for a small local runtime but not sufficient for a large multi-client production system.

---

## 5.7 src/vision_sst/engine/store.py

### Purpose
Persists session data in JSON.

### Implementation details
- Uses a default path in the system temp directory unless overridden.
- Reads existing JSON if present and loads it into memory.
- Saves after each add/update.

### Data shape
The store writes a dictionary with a sessions key containing session dictionaries.

### Why it matters
This is the persistence boundary for runtime session state.

---

## 5.8 src/vision_sst/engine/history.py

### Purpose
Persists session snapshots, separate from the session store, for history-like tracking.

### Structure
- SessionSnapshot dataclass used to represent a history item.
- HistoryStore stores a list of snapshots.

### Why it matters
The engine can separate operational state from historical traces, which is helpful for analytics or debugging.

---

## 5.9 src/vision_sst/engine/grpc_server.py

### Purpose
Provides a gRPC server implementation for the engine service.

### Responsibilities
- Create a gRPC server instance.
- Attach the EngineServiceServicer.
- Expose methods for StartSession, StopSession, GetStatus, StreamEvents, and GetHistory.

### Why it matters
This is the transport layer that lets remote clients talk to the engine over a real RPC boundary.

---

## 5.10 src/vision_sst/engine/grpc_client.py

### Purpose
Provides a convenience wrapper around the generated gRPC stub.

### Key methods
- start_session()
- stop_session()
- get_status()
- stream_events()
- get_history()
- close()

### Why it matters
This allows client code to interact with the engine without dealing directly with protobuf details.

---

## 5.11 src/vision_sst/__main__.py

### Purpose
CLI entrypoint that launches the gRPC engine.

### Runtime behavior
- Parses an optional --port argument.
- Uses VISION_SST_PORT environment variable when present.
- Builds and starts a gRPC server.

### Why it matters
This is the main executable path for starting the service in a local or container environment.

---

## 5.12 src/vision_sst/ui.py

### Purpose
Generates the HTML for the local dashboard.

### Responsibilities
- Build a status payload from the engine service.
- Render a polished HTML page with summary cards and dynamic panels.
- Provide JavaScript callbacks for refresh/start/stop actions.

### Why it matters
The dashboard acts as a lightweight local control plane for viewing runtime health and session state.

---

## 5.13 src/vision_sst/ui_server.py

### Purpose
Hosts the dashboard over HTTP and exposes simple JSON endpoints.

### API surface
- GET /: serves the dashboard page
- GET /api/status: returns current runtime state, events, and sessions
- POST /api/start: starts a new session
- POST /api/stop: stops the latest active session

### Why it matters
This makes the UI interactive without requiring a full frontend framework.

---

## 6. Data Flow

A typical runtime flow looks like this:

1. The user or client starts a session via EngineService.start_session().
2. The service creates a SessionRecord and persists it.
3. The service emits a system:session_started event.
4. Audio or other components can emit additional events into the engine service.
5. The UI polls /api/status and displays the latest state, events, and sessions.
6. The engine service can be queried over gRPC by clients.

### Example lifecycle
- Start -> session record created -> system event emitted -> state machine may transition from IDLE to ARMED depending on external triggers.
- Stop -> session marked stopped and persisted.

---

## 7. Testing Strategy

The repository uses pytest for unit tests. Current tests cover:

- engine service behavior
- UI rendering and payload generation
- Docker-related scaffolding

### Testing conventions
- Tests are simple and focus on behavior rather than implementation details.
- The project uses a source-tree import path configured in pyproject.toml.

---

## 8. Deployment and Runtime

### Running the engine
Use:

- python -m vision_sst --port 50051

### Running the UI
Use:

- python -m vision_sst.ui_server

### Docker
Use:

- docker compose up --build

### Environment variables
The runtime uses environment variables for configurability:

- VISION_SST_PORT
- VISION_SST_SESSION_STORE_PATH
- VISION_SST_HISTORY_STORE_PATH

---

## 9. Extension Points

### Add a real SST backend
Implement SSTPlugin and register it through the registry.

### Add a real LLM backend
Implement LLMPlugin and wire it into the engine service or FSM.

### Add real audio capture
Replace the current AudioService scaffold with a backend that reads from microphone devices or a streaming source.

### Add richer gRPC methods
Extend the protobuf definitions and the servicer implementation.

### Improve UI 
The existing UI is intentionally simple. It can be upgraded to a React/Vue app or kept as a lightweight HTML dashboard.

---

## 10. Junior Developer Guide

If you are new to this codebase, focus on the following in order:

1. Read the event model in events.py.
2. Understand the state machine in state_machine.py.
3. Trace the engine service in engine/service.py.
4. Review the UI server and dashboard in ui_server.py and ui.py.
5. Run the tests and inspect how the current behaviors are exercised.

### Suggested mental model
Think of the project as three layers:

- Runtime core: state machine + service + events
- Persistence: stores for sessions and history
- Interface layer: gRPC and UI server

### Good first tasks
- Add a new event type
- Extend the FSM with a new state/transition
- Add a new UI field driven by service data
- Add persistence of new session metadata

---

## 11. Senior Developer Guide

For senior contributors, the key design questions are:

1. How should the runtime evolve from a local scaffold into a production-grade service?
2. Where should concurrency and fault tolerance be improved?
3. How should the plugin architecture be hardened?
4. How should the event model and transport layers be made more robust, versioned, and observable?
5. How should persistence be migrated from JSON files to a durable backend if the system scales?

### Architectural considerations
- Replace the current in-memory event store with a proper queue or stream if the system grows.
- Introduce structured logging and metrics around state transitions and events.
- Make the FSM an explicit domain abstraction with a richer action pipeline.
- Move to a more robust RPC and auth model for remote deployment.
- Consider schema versioning for the JSON store and protobuf definitions.

### Review priorities
- Plugin lifecycle correctness
- Thread-safety and concurrency guarantees
- Backward compatibility of event and RPC schemas
- Observability and operational readiness

---

## 12. Summary

This repository is a compact but extensible foundation for a dictation engine. Its strengths are:

- simple architecture
- clean separation of concerns
- support for gRPC and a local web UI
- event-driven design that can grow into a full pipeline

Its main gaps are that it is still a scaffold and not yet a production speech system. The fastest route to maturity is to replace the placeholders with real audio capture, real speech-to-text, and robust persistence and deployment mechanisms.
