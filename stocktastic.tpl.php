<?php

/**
 * @file
 * Displays a filter input box to filter the table, and some averages for the year.
 */
?>

<div id="stocktastic-header">
  <div class="col-xs-6">
    <label for="filter">Filter the table</label>
    <input type="text" id="table-filter" placeholder="Type to filter" class="form-control input-normal" />
  </div>
  <div class="col-xs-6">
    <p>Average yearly movements:</p>
    <table>
      <tr><td>All stocks</td><td><?php print $stocks_average; ?>%</td></tr>
      <tr><td>Contestants</td><td><?php print $contestants_average; ?>%</td></tr>
      <tr><td>Brokers</td><td><?php print $brokers_average; ?>%</td></tr>
    </table>
  </div>
</div>
