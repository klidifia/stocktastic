<?php

/**
 * @file
 * Functions to assist with the retrieval of stock data from Yahoo! Finance.
 */

/**
 * Implement a HTTP request to Yahoo! Finance to retrieve stock quotes.
 *
 * @param $symbols array
 *   An array of ticker symbols to send to Yahoo!. There is not limit on the
 *   number of symbols. Yahoo! limits 200 symbols per request, so we queue up
 *   the request into batches.
 *
 * @return array
 *   An array of stocks with stock information fromm Yahoo!. False on failure.
 */
function stocktastic_fetch($symbols) {
  // Yahoo! limits stock updates to 200 or less per request. Split it up if needed.
  $l = 199;
  if (count($symbols) <= $l) {
    $s = array_map('urlencode', $symbols);
    $s = implode('+', $s);
    $fields = variable_get('stocktastic_data', 'sl1');
    $url = "http://download.finance.yahoo.com/d/quotes.csvr?s=$s&f=$fields&e=.csv";
    $results = drupal_http_request($url);
    if (_stocktastic_request_failure($results))
      return FALSE;

    // Symbols are returned in CSV format.
    return _stocktastic_parse_csv($results->data);
  }
  else {
    $batches = array_chunk($symbols, $l);
    $stocks = array();
    foreach ($batches as $key => $batch) {
      $stocks = array_merge(stocktastic_fetch($batch), $stocks);
    }
    return $stocks;
  }
}

/**
 * Save a single stock quote to the database.
 *
 * @param $stock object
 *   The stock objects keys' must match the field names of the database table.
 *   The stock objects values' are the data to insert.
 *
 * @return bool
 */
function stocktastic_save($stock) {
  // Fix potential text in float fields.
  $stock = _stocktastic_fix_floats($stock);

  $record = db_merge('stocktastic_stocks')
      ->key(array('symbol' => $stock->symbol))
      ->fields((array) $stock)
      ->execute();

  return $record;
}

/**
 * Return ticker information for a single symbol from our database, not Yahoo!
 *
 * @param $symbol string
 *   The ticker symbol to retrieve information for.
 * @param $q string
 *   The type of information to retrieve. Can be either: basic, extended,
 *   or real-time. The default is the site-wide setting, which is usually the
 *   best bet here.
 *
 * @return object
 */
function stocktastic_load($symbol, $q = NULL) {
  static $stocks = array();

  if (!drupal_strlen($symbol)) {
    return array();
  }

  if (!isset($stocks[$symbol])) {
    $fields = _stocktastic_get_fields($q);
    $results = db_query("SELECT " . implode(',', $fields) . " FROM {stocktastic_stocks} WHERE symbol = :symbol", array(':symbol' => $symbol), array('fetch' => PDO::FETCH_ASSOC));
    if (count($results)) {
      foreach ($results as $result) {
        $stocks[$symbol] = $result;
      }
    }
  }
  else {
    $stocks[$symbol] = array();
  }

  return $stocks[$symbol];
}

/**
 * Return ticker information for a batch of symbols from our database, not Yahoo!
 *
 * @param $symbol array
 *   An array of ticker symbols to retrieve information for.
 * @param $q string
 *   The type of information to retrieve. Can be either: basic, extended,
 *   or real-time. The default is the site-wide setting, which is usually the
 *   best bet here.
 *
 * @return array
 *   An array of stocks, keyed by their ticker symbol. Each stock is an object within the array.
 */
function stocktastic_multiload($symbol, $q = NULL) {
  $stocks = array();
  $fields = _stocktastic_get_fields($q);
  $symbols = implode(', ', array_map('_stocktastic_quote_it', $symbol));
  $results = db_query("SELECT " . implode(', ', $fields) . " FROM {stocktastic_stocks} WHERE symbol IN ({$symbols})");
  foreach ($results as $stock) {
    $stocks[$stock->symbol] = $stock;
  }

  return $stocks;
}

/**
 * Wrap a string in quotes. Usually used with array_map().
 */
function _stocktastic_quote_it($string, $style = 'single') {
  return ($style != 'single') ? '"' . $string . '"' : "'" . $string . "'";
}

/**
 * Convert a stock from Yahoo! to its object oriented counterpart.
 *
 * @param $stock
 * @param null $q
 *
 * @return object
 */
function _stocktastic_to_object($stock, $q = NULL) {
  $fields = _stocktastic_get_fields($q);
  $ns = count($fields);
  if ($ns) {
    $s = new stdClass();
    for ($i = 0; $i < $ns; $i++) {
      $s->$fields[$i] = $stock[$i];
    }
  }
  if (isset($s)) {
    $s->updated = REQUEST_TIME;
  }

  return $s;
}

/**
 * Generate the actual database field names currently active. These field names usually double as the keys for the
 * stock object.
 */
function _stocktastic_get_fields($q = NULL) {
  if (!$q) {
    $q = variable_get('stocktastic_data', 'sl1');
  }
  preg_match_all('/[a-z][0-9]?/', $q, $yfields);
  $yfields = $yfields[0];

  $map = _stocktastic_yahoo_map();

  $ns = count($yfields);
  $s = array();
  for ($i = 0; $i < $ns; $i++) {
    $dbname = $map[$yfields[$i]];
    $s[] = $dbname;
  }

  return $s;
}

/**
 * Return an array of Yahoo! fields keys mapped to the database field names.
 *
 * Note: In stocktastic.install there is a special key added to the database
 * install schema to map the Yahoo! field name to our database field names. The
 * new key is named 'yahoo'.
 */
function _stocktastic_yahoo_map() {
  static $map = NULL;
  if (!isset($map)) {
    $schema = drupal_get_schema('stocktastic_stocks');
    foreach ($schema['fields'] as $name => $field) {
      if (isset($field['yahoo'])) {
        $y = $field['yahoo'];
        $map[$y] = $name;
      }
    }
  }
  return $map;
}

/**
 * Helper function to convert a feed from Yahoo! to an array.
 */
function _stocktastic_parse_csv($data) {
  $stocks = array();
  $lines = explode("\n", trim($data));
  // Clean up the data.
  foreach ($lines as $key => $symbol) {
    $stock = str_getcsv($symbol);
    foreach ($stock as $key => $value) {
      // Clean up and normalize the data
      $value = trim($value, '"');
      if ($value == '0.00')
        $value = 0;
      elseif ($value == '0.00%')
        $value = 0;
      $stock[$key] = $value;
    }
    $stocks[] = $stock;
  }
  return $stocks;
}

/**
 * Internal helper function to deal cleanly with various HTTP response codes.
 */
function _stocktastic_request_failure($results) {
  switch ($results->code) {
    case '200': // Success!
    case '304': // Not modified, nothing to do.
      return FALSE;
    default:
      watchdog('stocktastic', 'Failed to retrieve stock quotes with error: %error', array('%error' => $results->error));
      return TRUE;
  }
}

/**
 * Internal helper function to change text values returned for float fields to '0'.
 */
function _stocktastic_fix_floats($stock) {
  $stock = (array) $stock;
  $schema = drupal_get_schema('stocktastic_stocks');

  foreach ($schema['fields'] as $name => $field) {
    if ($field['type'] == 'float' && isset($stock[$name]) && !is_numeric($stock[$name])) {
      $stock[$name] = 0;
    }
  }

  return (object) $stock;
}

/**
 * str_getcsv() handles fields with commas better but it's only in PHP 5 >= 5.3.0.
 */
if (!function_exists('str_getcsv')) {
  function str_getcsv($input, $delimiter = ',') {
    $tempfile = tmpfile();
    fwrite($tempfile, $input);
    rewind($tempfile);
    $csv_length = drupal_strlen($input);
    $output = fgetcsv($tempfile, $csv_length, $delimiter);
    fclose($tempfile);
    return $output;
  }
}
