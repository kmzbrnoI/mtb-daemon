"""
Common test functions
"""


def check_version_format(version: str) -> None:
    versions = version.split('.')
    assert len(versions) == 2
    major, minor = versions
    assert major.isdigit()
    assert minor.isdigit()
