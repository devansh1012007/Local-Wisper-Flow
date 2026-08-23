from vision_sst.__main__ import main


def test_main_invokes_server(monkeypatch, capsys):
    started = []

    class DummyServer:
        def add_insecure_port(self, target):
            started.append(target)
            return 50051

        def start(self):
            started.append("start")

        def wait_for_termination(self):
            started.append("wait")

    monkeypatch.setattr("vision_sst.__main__.create_server", lambda: DummyServer())
    monkeypatch.setattr("sys.argv", ["vision_sst", "--port", "50051"])

    main()

    assert started[0].endswith("50051")
    assert "Engine listening on port 50051" in capsys.readouterr().out
