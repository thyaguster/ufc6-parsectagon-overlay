"""Safety checks for the reversible FightStatsHelper probe."""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import fight_stats_probe as probe


def main():
    source = Path(r"E:\Jogos\UFC 6 v01.006\PPSA23566-app0-files\Data\Ps5\superbundlelayout\ufc_installpackage_00\cas_06.cas")
    with source.open("rb") as stream:
        stream.seek(probe.OFFSET)
        original = stream.read(probe.LENGTH)
    assert probe.digest(original) == probe.ORIGINAL_SHA256
    patched = probe.replacement(original)
    assert len(patched) == probe.LENGTH
    with tempfile.TemporaryDirectory() as folder:
        archive = Path(folder) / "cas_06.cas"
        backup = Path(folder) / "backup.bin"
        with archive.open("wb") as stream:
            stream.seek(probe.OFFSET)
            stream.write(original)
        game_running = probe.core.game_running
        probe.core.game_running = lambda: False
        try:
            assert probe.run("status", archive, backup) == "original"
            assert probe.run("install", archive, backup) == "probe installed"
            assert backup.read_bytes() == original
            assert probe.run("status", archive, backup) == "probe installed"
            assert probe.run("restore", archive, backup) == "original"
            with archive.open("rb") as stream:
                stream.seek(probe.OFFSET)
                assert stream.read(probe.LENGTH) == original
        finally:
            probe.core.game_running = game_running
    print("fight stats probe backup and restore passed")


if __name__ == "__main__":
    main()
