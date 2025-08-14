#!/usr/bin/env python3
import psycopg2
import json
import getpass
import re

def normalize_sn(sn):
    # Remove all non-hex characters and convert to lowercase
    return re.sub(r'[^0-9a-fA-F]', '', sn).lower()

def get_adc_order(dns):
    # Extract the last number after 'adc-'
    match = re.search(r'adc-(\d+)$', dns)
    return int(match.group(1)) if match else None

def main():
    # Database connection parameters
    host = 'db-nica.jinr.ru'
    port = 5432
    dbname = 'mpdmc_db'

    # Prompt for username and password at runtime
    user = input('Enter database username: ')
    password = getpass.getpass('Enter database password: ')

    # Connect to the database
    conn = psycopg2.connect(
        host=host,
        port=port,
        dbname=dbname,
        user=user,
        password=password
    )
    cur = conn.cursor()

    # Query: Get ADC table (all columns)
    cur.execute('SELECT * FROM ecal_pas."ADC"')
    adc_rows = cur.fetchall()
    adc_columns = [desc[0] for desc in cur.description]
    adc_data = [dict(zip(adc_columns, row)) for row in adc_rows]

    # Process data to create adcMap.json structure
    baskets = {}
    for adc in adc_data:
        ehs = str(adc.get('ehs', '')).strip()
        if not ehs.isdigit():
            continue  # Skip if ehs is empty or not a digit
        basket_num = int(ehs)
        order = get_adc_order(str(adc.get('dns', '')))
        sn = normalize_sn(str(adc.get('sn', '')))
        if basket_num not in baskets:
            baskets[basket_num] = {}
        if order is not None:
            baskets[basket_num][str(order)] = sn

    # Convert to required output format: list of dicts
    output = []
    for basket_num in sorted(baskets):
        basket_dict = {'basket': basket_num}
        for order in sorted(baskets[basket_num], key=lambda x: int(x)):
            basket_dict[order] = baskets[basket_num][order]
        output.append(basket_dict)


    # Save main file
    with open('adcMap.json', 'w') as f:
        json.dump(output, f, indent=2)

    # Save snapshot with timestamp
    from datetime import datetime
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    snap_filename = f'adcMap_snap_{timestamp}.json'
    with open(snap_filename, 'w') as f:
        json.dump(output, f, indent=2)

    cur.close()
    conn.close()
    print(f"Data saved to adcMap.json and {snap_filename}")

if __name__ == '__main__':
    main()
