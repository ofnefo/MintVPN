"""Copy Quattro configuration databases into a new, inactive MintVPN profile.

This copies SQLite databases only; it does not migrate file assets, launch the
client, or disable a remembered profile. Treat the result as sensitive.
"""
import argparse
from contextlib import closing
from pathlib import Path
import sqlite3


def reject_links(path: Path) -> None:
    # Check before resolve(): Windows junctions are directory reparse points.
    for component in (path, *path.parents):
        if component.is_symlink() or component.is_junction():
            raise ValueError('junctions and symlinks are not supported')


def snapshot(source: Path, destination: Path) -> list[str]:
    reject_links(source.absolute())
    reject_links(destination.absolute())
    source = source.resolve(strict=True)
    destination = destination.resolve(strict=False)
    if not source.is_dir() or source == destination or source in destination.parents:
        raise ValueError('source must be a directory distinct from destination')
    config = source / 'config'
    reject_links(config)
    primary = config / 'quattro.db'
    if not primary.is_file() or primary.is_symlink():
        raise ValueError('source config/quattro.db is missing or is a symlink')
    databases = [primary]
    stats = config / 'quattro_stats.db'
    if stats.exists():
        if not stats.is_file() or stats.is_symlink():
            raise ValueError('source statistics database is not a regular file')
        databases.append(stats)

    # Exclusive creation avoids replacing an existing profile. Failures leave a
    # partial directory for inspection; never delete a potentially valuable copy.
    destination.mkdir(parents=False, exist_ok=False)
    target_config = destination / 'config'
    target_config.mkdir()
    copied = []
    for database in databases:
        uri = database.resolve(strict=True).as_uri() + '?mode=ro'
        target = target_config / database.name
        with closing(sqlite3.connect(uri, uri=True)) as src, closing(sqlite3.connect(target)) as dst:
            src.backup(dst)
            if dst.execute('PRAGMA integrity_check').fetchone() != ('ok',):
                raise RuntimeError('snapshot integrity check failed')
        copied.append(database.name)
    return copied


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    copied = snapshot(args.source, args.destination)
    print('Copied database files:', ', '.join(copied))
    print('Inactive copy only; configuration may contain secrets and remembered auto-start.')
