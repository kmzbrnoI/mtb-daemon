"""
Common test functions
"""

from typing import Dict, Any


def check_version_format(version: str) -> None:
    versions = version.split('.')
    assert len(versions) == 2
    major, minor = versions
    assert major.isdigit()
    assert minor.isdigit()


def check_error(response: Dict[str, Any], error_id: int) -> None:
    assert 'status' in response
    assert response['status'] == 'error'
    assert 'error' in response
    error = response['error']

    assert 'code' in error
    assert isinstance(error['code'], int)
    assert response['error']['code'] == error_id

    assert 'message' in error
    assert isinstance(error['message'], str)
    assert error['message'] != ''
