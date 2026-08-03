from vision_sst.engine.service import EngineService

service = EngineService()
session = service.start_session(mode='toggle', language='en')
print('sessions', [s.id for s in service._sessions])
print('store list', service._store.list_sessions())
service.stop_session(session.id)
print('history', service.get_history(limit=5))
