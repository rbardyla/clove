#!/usr/bin/env python3
"""
Fetch Handmade Hero YouTube transcripts
"""

from youtube_transcript_api import YouTubeTranscriptApi
import json
import sys

# Example Handmade Hero video IDs (replace with actual ones)
HANDMADE_HERO_VIDEOS = {
    "day_001": "Kz0KuEfkXEo",  # Day 001 - Setting Up the Windows Build
    "day_002": "ABCD1234567",  # Replace with actual video ID
    # Add more video IDs here
}

def fetch_transcript(video_id, video_name):
    """Fetch transcript for a single video"""
    try:
        # Create API instance
        ytt_api = YouTubeTranscriptApi()
        
        # Fetch the transcript
        transcript = ytt_api.fetch(video_id)
        
        # Save to file
        filename = f"transcripts/{video_name}.json"
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump({
                "video_id": video_id,
                "name": video_name,
                "snippets": [
                    {
                        "text": snippet.text,
                        "start": snippet.start,
                        "duration": snippet.duration
                    }
                    for snippet in transcript.snippets
                ]
            }, f, indent=2)
        
        print(f"✓ Saved {video_name} transcript to {filename}")
        return True
        
    except Exception as e:
        print(f"✗ Failed to fetch {video_name}: {e}")
        return False

def main():
    """Fetch all Handmade Hero transcripts"""
    import os
    
    # Create transcripts directory
    os.makedirs("transcripts", exist_ok=True)
    
    success_count = 0
    for name, video_id in HANDMADE_HERO_VIDEOS.items():
        if fetch_transcript(video_id, name):
            success_count += 1
    
    print(f"\nFetched {success_count}/{len(HANDMADE_HERO_VIDEOS)} transcripts")

if __name__ == "__main__":
    # Install required package:
    # pip install youtube-transcript-api
    main()