#!/usr/bin/env python3
"""Simple LLM client for Sish - avoids heredoc deadlock in shell"""
import json
import sys
import urllib.error
import urllib.request

def main():
    if len(sys.argv) < 5:
        print("Usage: llm_client.py <url> <payload> <connect_timeout> <request_timeout>", file=sys.stderr)
        sys.exit(2)

    url = sys.argv[1]
    payload_str = sys.argv[2]
    connect_timeout = max(1, int(sys.argv[3]))
    request_timeout = max(10, int(sys.argv[4]))

    req = urllib.request.Request(
        url,
        data=payload_str.encode(),
        headers={"Content-Type": "application/json"},
        method="POST"
    )

    try:
        with urllib.request.urlopen(req, timeout=request_timeout) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace") if e.fp else str(e)
        print(body or str(e), file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(str(e), file=sys.stderr)
        sys.exit(1)

    if not raw.strip():
        print("No response from LLM.", file=sys.stderr)
        sys.exit(1)

    try:
        data = json.loads(raw)
        choices = data.get("choices") if isinstance(data, dict) else None
        if isinstance(choices, list) and choices:
            first = choices[0]
            if isinstance(first, dict):
                msg = first.get("message", {})
                if isinstance(msg, dict):
                    content = msg.get("content", "")
                    if isinstance(content, str) and content.strip():
                        print(content.strip())
                        sys.exit(0)
        # Fallback: print raw response
        print(raw.strip())
    except json.JSONDecodeError:
        print(raw.strip())

if __name__ == "__main__":
    main()
