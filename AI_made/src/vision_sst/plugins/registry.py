from __future__ import annotations

from pathlib import Path
from typing import Dict, List

from vision_sst.plugins.base import Capability, LLMPlugin, SSTPlugin, TTSPlugin


class PluginRegistry:
    def __init__(self):
        self._sst: Dict[str, SSTPlugin] = {}
        self._tts: Dict[str, TTSPlugin] = {}
        self._llm: Dict[str, LLMPlugin] = {}

    def register_sst(self, name: str, plugin: SSTPlugin) -> None:
        self._sst[name] = plugin

    def register_tts(self, name: str, plugin: TTSPlugin) -> None:
        self._tts[name] = plugin

    def register_llm(self, name: str, plugin: LLMPlugin) -> None:
        self._llm[name] = plugin

    def get_sst(self, name: str) -> SSTPlugin:
        if name not in self._sst:
            raise KeyError(f"SST plugin '{name}' not found. Available: {list(self._sst.keys())}")
        return self._sst[name]

    def list_sst(self) -> List[Capability]:
        return [plugin.capability for plugin in self._sst.values()]

    def discover(self, paths: List[Path]) -> None:
        for path in paths:
            if not path.exists():
                continue
            for entry in path.iterdir():
                if entry.is_file() and entry.suffix == ".py" and entry.name != "__init__.py":
                    module_name = entry.stem
                    module = __import__(f"vision_sst.plugins.{module_name}", fromlist=["register"])
                    if hasattr(module, "register"):
                        module.register(self)
