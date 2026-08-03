from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

import numpy as np


@dataclass(frozen=True)
class Capability:
    name: str
    version: str
    requires_gpu: bool = False
    offline_capable: bool = True
    latency_ms: Optional[int] = None


@dataclass(frozen=True)
class TranscriptionResult:
    text: str
    confidence: float
    language: str
    word_timings: List[Dict[str, Any]]
    is_final: bool = True


class SSTPlugin(ABC):
    @property
    @abstractmethod
    def capability(self) -> Capability:
        ...

    @abstractmethod
    def load(self, config: Dict[str, Any]) -> bool:
        """Return True on success. Must be idempotent."""
        ...

    @abstractmethod
    def transcribe(self, audio: np.ndarray, context: Dict[str, Any]) -> TranscriptionResult:
        ...

    @abstractmethod
    def unload(self) -> None:
        ...

    @property
    def supports_streaming(self) -> bool:
        return False


class TTSPlugin(ABC):
    @property
    @abstractmethod
    def capability(self) -> Capability:
        ...

    @abstractmethod
    def load(self, config: Dict[str, Any]) -> bool:
        ...

    @abstractmethod
    def synthesize(self, text: str, context: Dict[str, Any]) -> bytes:
        ...

    @abstractmethod
    def unload(self) -> None:
        ...


class LLMPlugin(ABC):
    @property
    @abstractmethod
    def capability(self) -> Capability:
        ...

    @abstractmethod
    def load(self, config: Dict[str, Any]) -> bool:
        ...

    @abstractmethod
    def process(self, text: str, context: Dict[str, Any]) -> str:
        ...

    @abstractmethod
    def unload(self) -> None:
        ...
