"""Signing configuration checks using synthetic profiles; no account access."""
import unittest
from tools.apple_cloud_config import cloud_resigning_entitlements


class CloudResigning(unittest.TestCase):
    def setUp(self):
        self.container = 'iCloud.example.Halo'
        self.allowed = {
            'com.apple.developer.icloud-container-identifiers': [self.container, 'iCloud.example.Other'],
            'com.apple.developer.icloud-services': ['CloudKit', 'CloudDocuments'],
            'com.apple.developer.icloud-container-environment': ['Development', 'Production'],
        }

    def test_only_requested_capabilities_are_granted(self):
        result = cloud_resigning_entitlements(self.container, self.allowed, 'Development')
        self.assertEqual(result['com.apple.developer.icloud-container-identifiers'], [self.container])
        self.assertEqual(result['com.apple.developer.icloud-services'], ['CloudKit'])
        self.assertEqual(result['com.apple.developer.icloud-container-environment'], 'Development')

    def test_requires_explicit_environment(self):
        with self.assertRaises(ValueError):
            cloud_resigning_entitlements(self.container, self.allowed, None)

    def test_unrelated_profile_rejected(self):
        with self.assertRaises(ValueError):
            cloud_resigning_entitlements('iCloud.someoneElse.Halo', self.allowed, 'Development')

    def test_production_not_granted_by_development_profile(self):
        self.allowed['com.apple.developer.icloud-container-environment'] = 'Development'
        with self.assertRaises(ValueError):
            cloud_resigning_entitlements(self.container, self.allowed, 'Production')
        self.assertTrue(cloud_resigning_entitlements(self.container, self.allowed, 'Development'))

    def test_missing_service_rejected(self):
        self.allowed['com.apple.developer.icloud-services'] = []
        with self.assertRaises(ValueError):
            cloud_resigning_entitlements(self.container, self.allowed, 'Development')

    def test_local_only(self):
        self.assertEqual(cloud_resigning_entitlements(None, {}, None), {})
        with self.assertRaises(ValueError):
            cloud_resigning_entitlements(None, {}, 'Development')


if __name__ == '__main__':
    unittest.main()
