<?php

/**
 * @file
 * Stocktastic administration pages.
 */

/**
 * Create the form for Stocktastic admin settings.
 */
function stocktastic_admin_settings() {
  $form = array();
  $form['stocktastic'] = array(
    '#prefix' => '<strong>Update frequency</strong>',
    '#markup' => '<p>' . t('Stocktastic updates on every cron run. ' . (l('Run cron', 'admin/reports/status/run-cron'))) . '.</p>'
  );

  $form['stocktastic_data'] = array(
    '#type' => 'textfield',
    '#title' => t('The fields of data to receive on the stocks.'),
    '#default_value' => variable_get('stocktastic_data', 'sl1'),
    '#description' => t('Refer to: http://greenido.wordpress.com/2009/12/22/yahoo-finance-hidden-api/'),
  );

  $form['stocktastic_entry_close'] = array(
    '#type' => 'textfield',
    '#title' => t('The closing time of the competition.'),
    '#default_value' => variable_get('stocktastic_entry_close', 1420023639),
    '#description' => t('Enter this in epoch time'),
  );

  $form['stocktastic_symbols'] = array(
    '#markup' => '<p>' . l('Enter the stock manager', 'admin/config/services/stocktastic/stocks') . '</p>',
  );

  return system_settings_form($form);
}

/**
 * Stock admin page.
 */
function stocktastic_stocks() {
  $output = '<p>' . l('Add a stock', 'admin/config/services/stocktastic/stocks/add') . '</p>';

  $query = db_select('stocktastic_stocks', 'ss')
    ->extend('PagerDefault')
    ->extend('TableSort');
  $result = $query->fields('ss', array('symbol', 'name'))
    ->limit(200)
    ->orderByHeader(array('symbol'))
    ->execute();

  $header = array(
    'symbol' => array('data' => t('Symbol'), 'field' => 's.symbol', 'class' => array('left')),
    'name' => array('data' => t('Name'), 'field' => 's.name', 'class' => array('left')),
  );

  $rows = array();
  while ($data = $result->fetchObject()) {
    $rows[] = array(
      l(stocktastic_symbol_truncate($data->symbol), 'admin/config/services/stocktastic/stocks/' . $data->symbol . '/edit'),
      $data->name
    );
  }

  $output .= theme_table(array('header' => $header, 'rows' => $rows, 'attributes' => array('width' => '100%', 'id' => 'stocktastic-table'), 'sticky' => TRUE, 'caption' => null, 'colgroups' => array(), 'empty' => t('No stocks.'))) . theme("pager");

  return $output;
}

/**
 * Implements hook_form().
 *
 * Stock addition form.
 */
function stocktastic_add_stock_form($form, &$form_state) {
  $form['symbol'] = array(
    '#type' => 'textfield',
    '#title' => t('Stock symbol'),
    '#description' => t('Remember to include the suffix for Yahoo! (e.g. \'.NZ\')'),
    '#default_value' => '.NZ',
    '#required' => TRUE,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add stock'),
  );

  return $form;
}

/**
 * Stock addition form validation.
 */
function stocktastic_add_stock_form_validate($form, &$form_state) {
  // Check that this stock does not already exist.
  $result = db_select('stocktastic_stocks', 'ss')
    ->fields('ss', array('symbol'))
    ->condition('ss.symbol', check_plain($form_state['values']['symbol']), '=')
    ->execute()
    ->fetch();

  if ($result) {
    form_set_error('symbol', t('The symbol @symbol already exists.', array(
      '@symbol' => $form_state['values']['symbol'],
    )));
  }
}

/**
 * Stock addition handler.
 */
function stocktastic_add_stock_form_submit($form, &$form_state) {
  db_insert('stocktastic_stocks')
    ->fields(array(
      'symbol' => $form_state['values']['symbol'],
    ))
    ->execute();

  drupal_set_message(t('Stock saved; it will be populated on cron. ' . l('Edit it now', 'admin/config/services/stocktastic/stocks/' . $form_state['values']['symbol'] . '/edit') . '.'));
  $form_state['redirect'] = 'admin/config/services/stocktastic/stocks';
}

/**
 * Implements hook_form().
 *
 * Form to edit or delete an existing stock.
 */
function stocktastic_edit_stock_form($form, &$form_state) {
  $result = db_select('stocktastic_stocks', 'ss')
    ->fields('ss', array('symbol', 'name', 'last_trade_price_only', 'year_start_price', 'dividends', 'manual_adjustments', 'year_percent_change'))
    ->condition('ss.symbol', arg(5), '=')
    ->execute()
    ->fetch();

  $form['symbol'] = array(
    '#prefix' => '<h1>',
    '#markup' => isset($result->symbol) ? $result->symbol : '',
    '#suffix' => '</h1>',
  );
  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#default_value' => isset($result->name) ? $result->name : '',
    '#required' => TRUE,
  );
  $form['last_trade_price_only'] = array(
    '#type' => 'textfield',
    '#title' => t('Last price'),
    '#default_value' => isset($result->last_trade_price_only) ? $result->last_trade_price_only : '0',
    '#required' => TRUE,
  );
  $form['year_start_price'] = array(
    '#type' => 'textfield',
    '#title' => t('Year start price'),
    '#default_value' => isset($result->year_start_price) ? $result->year_start_price : '0',
    '#required' => TRUE,
  );
  $form['dividends'] = array(
    '#type' => 'textfield',
    '#title' => t('Dividends'),
    '#default_value' => isset($result->dividends) ? $result->dividends : '0',
    '#required' => TRUE,
  );
  $form['manual_adjustments'] = array(
    '#type' => 'textfield',
    '#title' => t('Manual adjustments'),
    '#default_value' => isset($result->manual_adjustments) ? $result->manual_adjustments : '0',
    '#required' => TRUE,
  );
  $form['year_percent_change'] = array(
    '#prefix' => '<p>Year percent change: ',
    '#markup' => isset($result->year_percent_change) ? $result->year_percent_change : '0',
    '#suffix' => '%</p>',
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Edit stock'),
  );
  $delete = user_access('delete stocktastic stocks');
  if (($delete) === TRUE) {
    $form['delete'] = array(
      '#type' => 'submit',
      '#value' => 'Delete',
      '#submit' => array('stocktastic_edit_stock_form_delete'),
    );
  }
  return $form;
}

/**
 * Stock edit handler.
 */
function stocktastic_edit_stock_form_submit($form, &$form_state) {
  db_update('stocktastic_stocks')
    ->fields(array(
      'name' => $form_state['values']['name'],
      'last_trade_price_only' => $form_state['values']['last_trade_price_only'],
      'year_start_price' => $form_state['values']['year_start_price'],
      'dividends' => $form_state['values']['dividends'],
      'manual_adjustments' => $form_state['values']['manual_adjustments'],
    ))
    ->condition('symbol', arg(5), '=')
    ->execute();

  drupal_set_message(t('Stock saved'));
}

/**
 * Stock deletion handler.
 */
function stocktastic_edit_stock_form_delete($form, &$form_state) {
  $delete = user_access('delete stocktastic stocks');
  if (($delete) !== TRUE) {
    return FALSE;
  }
  db_delete('stocktastic_stocks')
    ->condition('symbol', arg(5), '=')
    ->execute();

  drupal_set_message(t('Stock deleted'));
  $form_state['redirect'] = 'admin/config/services/stocktastic/stocks';
}

/**
 * Dividend admin page.
 */
function stocktastic_dividends() {
  $output = '<p>' . l('Add a dividend', 'admin/config/services/stocktastic/dividends/add') . '</p>';

  $query = db_select('stocktastic_dividends', 'sd')
    ->extend('PagerDefault')
    ->extend('TableSort');
  $result = $query->fields('sd')
    ->limit(200)
    ->orderBy('symbol', 'ASC')
    ->execute();

  $header = array(
    'symbol' => array('data' => t('Stock')),
    'ex-dividend' => array('data' => t('Ex-dividend')),
    'payable' => array('data' => t('Payable')),
    'period' => array('data' => t('Period')),
    'amount' => array('data' => t('Amount'), 'style' => 'text-align: right'),
    'supp' => array('data' => t('Supp'), 'style' => 'text-align: right'),
    'imputation' => array('data' => t('Imputation'), 'style' => 'text-align: right'),
  );

  $rows = array();
  while ($data = $result->fetchObject()) {
    $rows[] = array(
      l(stocktastic_symbol_truncate($data->symbol), 'admin/config/services/stocktastic/dividends/' . $data->id . '/edit'),
      format_date($data->ex_dividend, 'custom', 'd/m/Y'),
      format_date($data->payable, 'custom', 'd/m/Y'),
      $data->period,
      stocktastic_format_align('$' . stocktastic_format_decimals($data->amount, 5)),
      stocktastic_format_align('$' . stocktastic_format_decimals($data->supp, 5)),
      stocktastic_format_align('$' . stocktastic_format_decimals($data->imputation, 5)),
    );
  }

  $output .= theme_table(array('header' => $header, 'rows' => $rows, 'attributes' => array('width' => '100%', 'id' => 'stocktastic-table'), 'sticky' => TRUE, 'caption' => null, 'colgroups' => array(), 'empty' => t('No dividends.'))) . theme("pager");

  return $output;
}

/**
 * Implements hook_form().
 *
 * Stock addition form.
 */
function stocktastic_add_dividend_form($form, &$form_state) {
  $form['symbol'] = array(
    '#type' => 'textfield',
    '#title' => t('Symbol'),
    '#description' => t('E.g., <strong>FBU.NZ</strong> for Fletcher\'s'),
    '#default_value' => '.NZ',
    '#required' => TRUE,
  );
  $today = date("Y-m-d H:i:s");
  $form['ex_dividend'] = array(
    '#type' => 'date_popup',
    '#title' => t('Ex-dividend date'),
    '#required' => TRUE,
    '#default_value' => $today,
    '#date_type' => DATE_DATETIME,
    '#date_timezone' => date_default_timezone(),
    '#date_format' => 'd/m/Y',
    '#date_increment' => 1,
    '#date_year_range' => '0:+1',
  );
  $form['payable'] = array(
    '#type' => 'date_popup',
    '#title' => t('Dividend payable date'),
    '#required' => FALSE,
    '#default_value' => $today,
    '#date_type' => DATE_DATETIME,
    '#date_timezone' => date_default_timezone(),
    '#date_format' => 'd/m/Y',
    '#date_increment' => 1,
    '#date_year_range' => '0:+1',
  );
  $form['period'] = array(
    '#type' => 'textfield',
    '#title' => t('Period'),
    '#description' => t('E.g., Interim, Final, Special'),
    '#default_value' => 'Interim',
    '#required' => FALSE,
  );
  $form['amount'] = array(
    '#type' => 'textfield',
    '#title' => t('amount'),
    '#default_value' => 0,
    '#required' => TRUE,
  );
  $form['supp'] = array(
    '#type' => 'textfield',
    '#title' => t('supp'),
    '#default_value' => 0,
    '#required' => TRUE,
  );
  $form['imputation'] = array(
    '#type' => 'textfield',
    '#title' => t('imputation'),
    '#default_value' => 0,
    '#required' => TRUE,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add dividend'),
  );

  return $form;
}

/**
 * Stock addition form validation.
 */
function stocktastic_add_dividend_form_validate($form, &$form_state) {

}

/**
 * Dividend addition handler.
 */
function stocktastic_add_dividend_form_submit($form, &$form_state) {
  $ex_dividend = DateTime::createFromFormat('Y-m-d', $form_state['values']['ex_dividend']);
  $payable = DateTime::createFromFormat('Y-m-d', $form_state['values']['payable']);
  db_insert('stocktastic_dividends')
    ->fields(array(
      'symbol' => $form_state['values']['symbol'],
      'ex_dividend' => $ex_dividend->getTimestamp(),
      'payable' => $payable->getTimestamp(),
      'period' => $form_state['values']['period'],
      'amount' => $form_state['values']['amount'],
      'supp' => $form_state['values']['supp'],
      'imputation' => $form_state['values']['imputation'],
    ))
    ->execute();

  drupal_set_message(t('Dividend saved.'));
  $form_state['redirect'] = 'admin/config/services/stocktastic/dividends';
}

/**
 * Implements hook_form().
 *
 * Dividend edit form.
 */
function stocktastic_edit_dividend_form($form, &$form_state) {
  $result = db_select('stocktastic_dividends', 'sd')
    ->fields('sd', array('symbol', 'ex_dividend', 'payable', 'period', 'amount', 'supp', 'imputation'))
    ->condition('sd.id', arg(5), '=')
    ->execute()
    ->fetch();

  $form['symbol'] = array(
    '#type' => 'textfield',
    '#title' => t('Symbol'),
    '#default_value' => isset($result->symbol) ? $result->symbol : '',
    '#required' => TRUE,
  );
  $form['ex_dividend'] = array(
    '#type' => 'date_popup',
    '#title' => t('ex_dividend'),
    '#default_value' => isset($result->ex_dividend) ? format_date($result->ex_dividend, 'custom', 'Y-m-d H:i:s') : '0',
    '#required' => TRUE,
    '#date_type' => DATE_DATETIME,
    '#date_timezone' => date_default_timezone(),
    '#date_format' => 'd/m/Y',
    '#date_increment' => 1,
    '#date_year_range' => '0:+1',
  );
  $form['payable'] = array(
    '#type' => 'date_popup',
    '#title' => t('payable'),
    '#default_value' => isset($result->payable) ? format_date($result->payable, 'custom', 'Y-m-d H:i:s') : '0',
    '#required' => FALSE,
    '#date_type' => DATE_DATETIME,
    '#date_timezone' => date_default_timezone(),
    '#date_format' => 'd/m/Y',
    '#date_increment' => 1,
    '#date_year_range' => '0:+1',
  );
  $form['period'] = array(
    '#type' => 'textfield',
    '#title' => t('period'),
    '#default_value' => isset($result->period) ? $result->period : '',
    '#required' => TRUE,
  );
  $form['amount'] = array(
    '#type' => 'textfield',
    '#title' => t('amount'),
    '#default_value' => isset($result->amount) ? $result->amount : '0.00',
    '#required' => TRUE,
  );
  $form['supp'] = array(
    '#type' => 'textfield',
    '#title' => t('supp'),
    '#default_value' => isset($result->supp) ? $result->supp : '0.00',
    '#required' => TRUE,
  );
  $form['imputation'] = array(
    '#type' => 'textfield',
    '#title' => t('imputation'),
    '#default_value' => isset($result->imputation) ? $result->imputation : '0.00',
    '#required' => TRUE,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Edit dividend'),
  );
  $form['delete'] = array(
    '#type' => 'submit',
    '#value' => 'Delete',
    '#submit' => array('stocktastic_edit_dividend_form_delete'),
  );

  return $form;
}

/**
 * Dividend edit handler.
 */
function stocktastic_edit_dividend_form_submit($form, &$form_state) {
  $ex_dividend = DateTime::createFromFormat('Y-m-d', $form_state['values']['ex_dividend']);
  $payable = DateTime::createFromFormat('Y-m-d', $form_state['values']['payable']);
  db_update('stocktastic_dividends')
    ->fields(array(
      'symbol' => $form_state['values']['symbol'],
      'ex_dividend' => $ex_dividend->getTimestamp(),
      'payable' => $payable->getTimestamp(),
      'period' => $form_state['values']['period'],
      'amount' => $form_state['values']['amount'],
      'supp' => $form_state['values']['supp'],
      'imputation' => $form_state['values']['imputation'],
    ))
    ->condition('id', arg(5), '=')
    ->execute();

  drupal_set_message(t('Dividend updated'));
  $form_state['redirect'] = 'admin/config/services/stocktastic/dividends';
}

/**
 * Dividend deletion handler.
 */
function stocktastic_edit_dividend_form_delete($form, &$form_state) {
  db_delete('stocktastic_dividends')
    ->condition('id', arg(5), '=')
    ->execute();

  drupal_set_message(t('Dividend deleted'));
  $form_state['redirect'] = 'admin/config/services/stocktastic/dividends';
}
