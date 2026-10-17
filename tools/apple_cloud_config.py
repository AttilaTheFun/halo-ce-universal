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


def cloud_resigning_entitlements(container, allowed, environment):
    """Use only the configured container, with an explicitly chosen database.

    Re-signing cannot silently switch between development and production saves.
    This checks profile authorization; it does not create a CloudKit container.
    """
    if not container:
        if environment:
            raise ValueError('This IPA has no configured iCloud container')
        return {}
    if environment not in ('Development', 'Production'):
        raise ValueError('A cloud-enabled IPA requires --cloud-environment Development or Production; use the same environment as the original build')
    containers = allowed.get('com.apple.developer.icloud-container-identifiers', [])
    if not isinstance(containers, list) or container not in containers:
        raise ValueError('The profile does not authorize this IPA’s iCloud container; use a matching profile or explicitly --disable-cloud-saves')
    services = allowed.get('com.apple.developer.icloud-services', [])
    if not isinstance(services, list) or 'CloudKit' not in services:
        raise ValueError('The profile does not authorize CloudKit')
    environments = allowed.get('com.apple.developer.icloud-container-environment', [])
    if isinstance(environments, str):
        environments = [environments]
    if not isinstance(environments, list) or environment not in environments:
        raise ValueError('The profile does not authorize the selected CloudKit environment')
    return {
        'com.apple.developer.icloud-container-identifiers': [container],
        'com.apple.developer.icloud-services': ['CloudKit'],
        'com.apple.developer.icloud-container-environment': environment,
    }
