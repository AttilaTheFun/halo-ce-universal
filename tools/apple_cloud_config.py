"""Validate build-time cloud opt-in without inspecting credentials or accounts."""
import plistlib
from pathlib import Path

def validate_cloud_config(container, entitlements, *, sandbox=False):
    if not container:
        return
    if not container.startswith('iCloud.') or not all(c.isascii() and (c.isalnum() or c in '.-') for c in container):
        raise ValueError('Use an explicit iCloud container identifier')
    if not entitlements:
        raise ValueError('Cloud saves require an explicit entitlements file and matching provisioning profile')
    with Path(entitlements).open('rb') as stream:
        values = plistlib.load(stream)
    if container not in values.get('com.apple.developer.icloud-container-identifiers', []):
        raise ValueError('The requested cloud container is absent from the entitlements')
    if 'CloudKit' not in values.get('com.apple.developer.icloud-services', []):
        raise ValueError('CloudKit is absent from the entitlements')
    if sandbox and values.get('com.apple.security.app-sandbox') is not True:
        raise ValueError('Mac cloud builds must retain App Sandbox')
