import sqlite3
import os
import subprocess
from contextlib import closing
import tempfile
import unittest
from pathlib import Path

from snapshot_config import snapshot


class SnapshotTests(unittest.TestCase):
    def test_online_wal_preserves_source_and_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            config = root / 'source' / 'config'
            config.mkdir(parents=True)
            connection = sqlite3.connect(config / 'quattro.db')
            try:
                connection.execute('PRAGMA journal_mode=WAL')
                connection.execute('PRAGMA wal_autocheckpoint=0')
                connection.execute('CREATE TABLE subscription (value TEXT)')
                connection.execute('INSERT INTO subscription VALUES (?)', ('synthetic',))
                connection.commit()
                self.assertTrue((config / 'quattro.db-wal').is_file())
                target = root / 'destination'
                self.assertEqual(snapshot(config.parent, target), ['quattro.db'])
                with closing(sqlite3.connect(target / 'config' / 'quattro.db')) as copied:
                    self.assertEqual(copied.execute('SELECT value FROM subscription').fetchall(), [('synthetic',)])
                self.assertEqual(connection.execute('SELECT value FROM subscription').fetchall(), [('synthetic',)])
                with self.assertRaises(FileExistsError):
                    snapshot(config.parent, target)
                with self.assertRaises(ValueError):
                    snapshot(config.parent, config.parent / 'nested')
            finally:
                connection.close()

    def test_missing_database_does_not_create_destination(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'source'
            source.mkdir()
            target = root / 'target'
            with self.assertRaises(ValueError):
                snapshot(source, target)
            self.assertFalse(target.exists())

    def test_source_config_junction_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'source'
            source.mkdir()
            other = root / 'other'
            other.mkdir()
            junction = source / 'config'
            result = subprocess.run(['cmd', '/c', 'mklink', '/J', str(junction), str(other)],
                                    capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
            if result.returncode:
                self.skipTest('cannot create test junction')
            try:
                with self.assertRaises(ValueError):
                    snapshot(source, root / 'target')
            finally:
                os.rmdir(junction)


if __name__ == '__main__':
    unittest.main()
