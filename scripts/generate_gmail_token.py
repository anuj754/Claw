#!/usr/bin/env python3
"""
Generate Gmail OAuth 2.0 Access Token using Device Authorization Flow.
It creates or updates google_workspace_config.json.
Requires a Google Cloud Console Desktop App OAuth client ID and secret.

Usage:
  python3 scripts/generate_gmail_token.py --client_id <ID> --client_secret <SECRET>
"""

import sys
import json
import time
import urllib.request
import urllib.parse
import argparse
import os

SCOPES = "https://mail.google.com/"
TOKEN_FILE = os.path.join(os.path.dirname(__file__), "..", "data", "config", "google_workspace_config.json")

def request_json(url, data=None):
    if data is not None:
        data = urllib.parse.urlencode(data).encode('utf-8')
    req = urllib.request.Request(url, data=data)
    try:
        with urllib.request.urlopen(req) as response:
            return json.loads(response.read().decode())
    except urllib.error.HTTPError as e:
        return json.loads(e.read().decode())

def generate_token(client_id, client_secret):
    print("Initiating Device Authorization Flow...")
    
    # 1. Request device code
    device_req_data = {
        'client_id': client_id,
        'scope': SCOPES
    }
    
    device_resp = request_json('https://oauth2.googleapis.com/device/code', device_req_data)
    
    if 'error' in device_resp:
        print(f"Error requesting device code: {device_resp}")
        return

    print("==================================================")
    print("Please visit the following URL on any device:")
    print(device_resp['verification_url'])
    print("\nAnd enter this code:")
    print(f"   {device_resp['user_code']}")
    print("==================================================")
    print("\nWaiting for authorization...")

    # 2. Poll for the token
    device_code = device_resp['device_code']
    interval = device_resp.get('interval', 5)
    
    token_req_data = {
        'client_id': client_id,
        'client_secret': client_secret,
        'device_code': device_code,
        'grant_type': 'urn:ietf:params:oauth:grant-type:device_code'
    }

    while True:
        time.sleep(interval)
        token_resp = request_json('https://oauth2.googleapis.com/token', token_req_data)
        
        if 'error' in token_resp:
            if token_resp['error'] == 'authorization_pending':
                continue
            elif token_resp['error'] == 'slow_down':
                interval += 2
                continue
            else:
                print(f"Error during authorization: {token_resp['error']}")
                return
        else:
            # Success!
            print("\nAuthorization successful!")
            save_token(token_resp)
            break

def save_token(token_resp):
    data_dir = os.path.dirname(TOKEN_FILE)
    if not os.path.exists(data_dir):
        os.makedirs(data_dir, exist_ok=True)
        
    config = {
        'access_token': token_resp['access_token'],
        'refresh_token': token_resp.get('refresh_token', ''),
        'expires_in': token_resp['expires_in'],
        'token_type': token_resp['token_type']
    }
    
    with open(TOKEN_FILE, 'w') as f:
        json.dump(config, f, indent=2)
        
    print(f"Token securely saved to {TOKEN_FILE}")
    print("The google-workspace-cli tool is now ready to use.")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate Gmail OAuth Token")
    parser.add_argument('--client_id', required=True, help='Google OAuth Client ID')
    parser.add_argument('--client_secret', required=True, help='Google OAuth Client Secret')
    
    args = parser.parse_args()
    generate_token(args.client_id, args.client_secret)
