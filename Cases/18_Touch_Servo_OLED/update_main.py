#!/usr/bin/env python3
"""
Update main.c with current date/time
"""
import re
from datetime import datetime

def main():
    with open('src/main.c', 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Get current date and time
    now = datetime.now()
    date_str = now.strftime('%Y-%m-%d')
    time_str = now.strftime('%H:%M:%S')
    
    print(f"Updating main.c with date: {date_str}, time: {time_str}")
    
    # Write back
    with open('src/main.c', 'w', encoding='utf-8') as f:
        f.write(content)
    
    print("Done!")

if __name__ == '__main__':
    main()
