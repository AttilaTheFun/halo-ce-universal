"""Validate build-time cloud opt-in without inspecting credentials or accounts."""
import plistlib
from pathlib import Path

def validate_cloud_config(container, entitlements, *, sandbox=False, backend="CloudKit"):
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
    service = 'CloudDocuments' if backend == 'GameSave' else 'CloudKit'
    if service not in values.get('com.apple.developer.icloud-services', []):
        raise ValueError(f'{service} is absent from the entitlements')
    if backend == 'GameSave' and container not in values.get('com.apple.developer.ubiquity-container-identifiers', []):
        raise ValueError('GameSave requires the matching iCloud Documents ubiquity container')
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


def game_save_resigning_entitlements(container, allowed):
    if not container:
        raise ValueError('This IPA has no configured GameSave container')
    for key in ('com.apple.developer.icloud-container-identifiers',
                'com.apple.developer.ubiquity-container-identifiers'):
        if not isinstance(allowed.get(key), list) or container not in allowed[key]:
            raise ValueError('The profile does not authorize the GameSave container; omit --enable-game-save for local-only play')
    if 'CloudDocuments' not in allowed.get('com.apple.developer.icloud-services', []):
        raise ValueError('The profile does not authorize iCloud Documents')
    return {
        'com.apple.developer.icloud-container-identifiers': [container],
        'com.apple.developer.ubiquity-container-identifiers': [container],
        'com.apple.developer.icloud-services': ['CloudDocuments'],
    }


def configure_resigned_cloud(info, allowed, environment=None, *, game_save=False):
    """Re-signed copies are local-only unless cloud is explicitly requested.

    Mutates the staged Info.plist only after validating the requested cloud
    capabilities. The source IPA and its imported data are never changed.
    """
    if game_save:
        if environment or info.get('HaloCloudBackend') != 'GameSave':
            raise ValueError('--enable-game-save requires a GameSave IPA, without --cloud-environment')
        return game_save_resigning_entitlements(info.get('HaloCloudContainer'), allowed)
    if environment is not None and info.get('HaloCloudBackend') == 'GameSave':
        raise ValueError('GameSave uses iCloud Documents; use --enable-game-save instead of --cloud-environment')
    if environment is None:
        info.pop('HaloCloudContainer', None)
        return {}
    return cloud_resigning_entitlements(info.get('HaloCloudContainer'), allowed, environment)
