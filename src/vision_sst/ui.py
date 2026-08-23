from __future__ import annotations

import json
from html import escape
from typing import Any

from vision_sst.engine.service import EngineService


def build_status_payload(service: EngineService | None = None) -> dict[str, Any]:
    engine = service or EngineService()
    status = engine.get_status()
    history = engine.get_history(limit=8)
    recent_events = []
    for event in reversed(engine.get_events(limit=8)):
        recent_events.append({
            "type": event.type,
            "timestamp": event.timestamp.isoformat(),
            "session_id": event.session_id,
        })
    sessions = []
    for session in sorted(engine._sessions, key=lambda item: item.started_at, reverse=True)[:8]:
        sessions.append({
            "id": session.id,
            "mode": session.mode,
            "status": session.status,
            "language": session.language,
            "started_at": session.started_at.isoformat(),
        })
    return {
        "state": status["state"],
        "model_loaded": status["model_loaded"],
        "audio_level": status["audio_level"],
        "session_count": len(engine._sessions),
        "history_count": len(history),
        "history": history,
        "events": recent_events,
        "sessions": sessions,
    }


def build_dashboard_html() -> str:
    payload = build_status_payload()
    return f"""
<!DOCTYPE html>
<html lang=\"en\">
  <head>
    <meta charset=\"utf-8\" />
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />
    <title>Vision SST Control Center</title>
    <style>
      :root {{
        color-scheme: dark;
        --bg: #07111f;
        --panel: rgba(8, 24, 43, 0.9);
        --border: rgba(255,255,255,0.12);
        --text: #f4f7fb;
        --muted: #88a0b8;
        --accent: #57c7ff;
        --accent-2: #7f8dff;
      }}
      * {{ box-sizing: border-box; }}
      body {{
        margin: 0;
        font-family: Inter, Segoe UI, sans-serif;
        background: radial-gradient(circle at top, #123252 0%, var(--bg) 45%, #040810 100%);
        color: var(--text);
        min-height: 100vh;
      }}
      .shell {{ max-width: 1160px; margin: 0 auto; padding: 32px 24px 48px; }}
      .hero {{ padding: 24px; border: 1px solid var(--border); border-radius: 24px; background: linear-gradient(135deg, rgba(87,199,255,0.15), rgba(127,141,255,0.15)); box-shadow: 0 20px 60px rgba(0,0,0,0.28); }}
      .hero h1 {{ margin: 0 0 8px; font-size: 2rem; }}
      .hero p {{ margin: 0; color: var(--muted); max-width: 680px; }}
      .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; margin-top: 22px; }}
      .card {{ background: var(--panel); border: 1px solid var(--border); border-radius: 18px; padding: 18px; }}
      .card h2 {{ font-size: 0.92rem; text-transform: uppercase; letter-spacing: .18em; color: var(--muted); margin: 0 0 8px; }}
      .metric {{ font-size: 1.8rem; font-weight: 700; margin: 6px 0 4px; }}
      .chip {{ display:inline-block; padding: 6px 10px; border-radius: 999px; background: rgba(87,199,255,.16); color: #8be1ff; font-size: .85rem; }}
      .actions {{ display:flex; gap: 12px; flex-wrap: wrap; margin-top: 22px; }}
      button {{ border: 0; border-radius: 999px; padding: 12px 16px; font-weight: 700; cursor: pointer; color: white; background: linear-gradient(90deg, var(--accent), var(--accent-2)); }}
      button.secondary {{ background: linear-gradient(90deg, #2f4b6d, #4f7aa8); }}
      .panel {{ margin-top: 22px; display:grid; grid-template-columns: 1.1fr 0.9fr; gap: 18px; }}
      .list {{ margin: 0; padding: 0; list-style: none; display:grid; gap:10px; }}
      .list li {{ padding: 12px 14px; border-radius: 12px; background: rgba(255,255,255,.04); border: 1px solid rgba(255,255,255,.06); }}
      table {{ width: 100%; border-collapse: collapse; }}
      th, td {{ text-align: left; padding: 10px 8px; border-bottom: 1px solid rgba(255,255,255,.08); font-size: 0.95rem; }}
      .status-pill {{ display:inline-block; padding: 5px 8px; border-radius: 999px; background: rgba(127,141,255,.16); color: #c5cbff; font-size: .8rem; }}
      .event-dot {{ width: 8px; height: 8px; border-radius: 50%; background: var(--accent); display:inline-block; margin-right: 8px; }}
      @media (max-width: 860px) {{ .panel {{ grid-template-columns: 1fr; }} }}
    </style>
  </head>
  <body>
    <div class=\"shell\">
      <section class=\"hero\">
        <h1>Vision SST Control Center</h1>
        <p>Monitor the engine, inspect sessions, and keep the dictation flow responsive from a polished local dashboard.</p>
        <div class=\"actions\">
          <button id=\"start-session\" type=\"button\">Start Listening</button>
          <button id=\"stop-session\" class=\"secondary\" type=\"button\">Stop Session</button>
          <button id=\"refresh-status\" class=\"secondary\" type=\"button\">Refresh</button>
        </div>
      </section>

      <section class=\"grid\">
        <div class=\"card\">
          <h2>Runtime State</h2>
          <div class=\"metric\" id=\"state\">{escape(str(payload['state']))}</div>
          <span class=\"chip\" id=\"model-loaded\">{escape(str(payload['model_loaded']))}</span>
        </div>
        <div class=\"card\">
          <h2>Audio Level</h2>
          <div class=\"metric\" id=\"audio-level\">{payload['audio_level']:.2f} dB</div>
          <span class=\"chip\">Live signal</span>
        </div>
        <div class=\"card\">
          <h2>Sessions</h2>
          <div class=\"metric\" id=\"session-count\">{payload['session_count']}</div>
          <span class=\"chip\">Tracked locally</span>
        </div>
        <div class=\"card\">
          <h2>History</h2>
          <div class=\"metric\" id=\"history-count\">{payload['history_count']}</div>
          <span class=\"chip\">Recent items</span>
        </div>
      </section>

      <section class=\"panel\">
        <div class=\"card\">
          <h2>Live Event Stream</h2>
          <ul id=\"event-list\" class=\"list\"></ul>
        </div>
        <div class=\"card\">
          <h2>Recent Sessions</h2>
          <table>
            <thead>
              <tr><th>ID</th><th>Status</th><th>Mode</th><th>Language</th></tr>
            </thead>
            <tbody id=\"session-table\"></tbody>
          </table>
        </div>
      </section>
    </div>
    <script>
      const refreshStatus = async () => {{
        const response = await fetch('/api/status');
        const data = await response.json();
        document.getElementById('state').textContent = data.state;
        document.getElementById('model-loaded').textContent = data.model_loaded ? 'Loaded' : 'Not loaded';
        document.getElementById('audio-level').textContent = data.audio_level.toFixed(2) + ' dB';
        document.getElementById('session-count').textContent = data.session_count;
        document.getElementById('history-count').textContent = data.history_count;

        const eventList = document.getElementById('event-list');
        eventList.innerHTML = '';
        (data.events || []).forEach((event) => {{
          const item = document.createElement('li');
          item.innerHTML = '<span class=\"event-dot\"></span>' + event.type + ' <span class=\"status-pill\">' + (event.session_id || 'system') + '</span>';
          eventList.appendChild(item);
        }});

        const sessionTable = document.getElementById('session-table');
        sessionTable.innerHTML = '';
        (data.sessions || []).forEach((session) => {{
          const row = document.createElement('tr');
          row.innerHTML = '<td>' + session.id.slice(0, 8) + '…</td><td><span class=\"status-pill\">' + session.status + '</span></td><td>' + session.mode + '</td><td>' + (session.language || '—') + '</td>';
          sessionTable.appendChild(row);
        }});
      }};

      document.getElementById('refresh-status').addEventListener('click', refreshStatus);
      document.getElementById('start-session').addEventListener('click', async () => {{
        await fetch('/api/start', {{ method: 'POST' }});
        await refreshStatus();
      }});
      document.getElementById('stop-session').addEventListener('click', async () => {{
        await fetch('/api/stop', {{ method: 'POST' }});
        await refreshStatus();
      }});
      document.addEventListener('keydown', (event) => {{
        if (event.key === 'r' && (event.ctrlKey || event.metaKey)) {{
          event.preventDefault();
          refreshStatus();
        }}
      }});
      refreshStatus();
    </script>
  </body>
</html>
"""
