<?php

function quickaudit_drush_command() {
  $items = array();
  $items['quickaudit-content'] = array(
    'description' => "Quick audit of a site's content",
    'options' => array(
      'oneline' => "Place all results on one line.",
      'compact' => "Use compact titles.",
      'showpcts' => "Show the percentages of nodes created and updated instead of graphic.",
      'showcount' => "Show the count of nodes created and updated instead of graphic.",
      'delimiter' => "Change delimiter between fields. Defaults to space.",
      'export' => "Prepare data for export to spreadsheet: no titles, oneline, tab delimited, show percenteages.",
    ),
    'examples' => array(
      'drush qa',
    ),
    'aliases' => array('qa-content'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  return $items;
}

function drush_quickaudit_drush_help() {
  switch ($section) {
    case 'drush:quickaudit-content':
      return dt("This command will do a quick audit of a site's content.");
      break;
  }
}

function drush_quickaudit_content() {

  // Find total nodes, in case there is nothing.
  $all_published_nodes = db_query("SELECT COUNT(*) FROM {node} WHERE status")->fetchField();
  if (!$all_published_nodes) {
    drush_print('No nodes found.');
    return;
  }
  $all_published_nodes_including_unpublished = db_query("SELECT COUNT(*) FROM {node}")->fetchField();
  
  // Find the starting date
  $curmonth = date('n');
  $lastyear = date('Y', strtotime('-1 year'));
  $startdate = mktime(0,0,0,$curmonth,1,$lastyear);

  // Find the total number of nodes within the time period
  $total_nodes = db_query("SELECT COUNT(*) FROM {node} WHERE status AND created >= :created", array(':created' => $startdate))->fetchField();
  
  global $base_url;
  
  $options = array();
  $options['export'] = $export = drush_get_option('export');
  $options['oneline'] = $oneline = drush_get_option('oneline', $export);
  $options['compact'] = $compact = drush_get_option('compact', $export);
  $options['showpcts'] = $showpcts = drush_get_option('showpcts', $export);
  $options['showcount'] = $showcount = drush_get_option('showcount');
  $delimiter_default = $oneline ? "\t" : "\n";
  $options['delimiter'] = $delimiter = drush_get_option('delimiter', $delimiter_default);

  $output['url'] = preg_replace('|^.*://|', '', $base_url);
  $output['total'] = $total_nodes;
  
  $then = new dateTime();
  $then->setTimestamp(db_query("SELECT min(created) FROM {users} WHERE created")->fetchField());
  $now = new dateTime();
  $now->setTimestamp(time());
  $interval = $then->diff($now);
  $site_longevity = ($interval->y * 12) + $interval->m + 1;
  
  // Get months
  $output['months'] = $site_longevity;
  
  // Get status
  if (module_exists('incubator')) {
    $output['status'] = variable_get('incubator_site_disabled', TRUE) ? 'disabled' : 'enabled';
  }
  
  // Get nodes per month
  $npm = round($total_nodes / $site_longevity, $compact ? 0 : 2);
  $output['npm'] = $export ? $npm : "$npm/m";
  
  $nodes_by_type = db_query("SELECT type, COUNT(nid) count FROM {node} "
      . "WHERE status AND created >= :created GROUP BY type ORDER BY count DESC", array(':created' => $startdate))->fetchAllKeyed();
  foreach ($nodes_by_type as $type => $count) {
    $pct = round($count / $total_nodes, 2);
    if ($pct > .30 || ($showpcts && count($output['types']) < 3)) {
      $output['types'][$type] = $showpcts ? $pct : $pct * 100 . '%';
    }
  }
  $type_names = implode(',', array_keys($output['types']));
  if ($export) {
    while (count($output['types']) < 3) {
      $output['types'][] = "0.00";
    }
  }
  $output['types']['types'] = $type_names;
  
  $nodes_by_user = db_query("SELECT u.name, COUNT(n.nid) count FROM {node} n LEFT JOIN {users} u ON n.uid = u.uid "
      . "WHERE n.status AND n.created >= :created GROUP BY u.name ORDER BY count DESC LIMIT 0,5", array(':created' => $startdate))->fetchAllKeyed();
  $x = 1;
  $authors = array();
  foreach ($nodes_by_user as $name => $count) {
    if ($x == 1) {
      $name = $compact ? 'user' : $name;
      $authors[$name] = $count;
      $authors['group'] = 0;
      $authors['other'] = 0;
    }
    else {
      $authors['group'] += $count;
    }
    $x++;
  }
  
  $authors['other'] = $total_nodes - ($authors['user'] + $authors['group']);
  
  if (empty($authors['group'])) {
    $authors['group'] = 0;
  }
  foreach ($authors as $name => $count) {
    $pct = round($count / $total_nodes, 2);
    $name = $compact ? substr($name, 0, 1) : $name;
    $output['authors'][$name] = $showpcts ? $pct : $pct * 100 . '%';
  }

  

  $num_months = min($site_longevity + 1, 13);
  
  $baseline = $npm < 6 ? $npm * 3 : $npm * 2;
  if ($baseline < 8) {
    $baseline = 8;
  }

  $output['create history'] = $output['update history'] = '';
  $nodes_created = $nodes_changed = _quickaudit_datearray($num_months);

  $nodes_created_counts = db_query('SELECT FROM_UNIXTIME(n.created, \'%Y-%m\') month, COUNT(n.nid) count
    FROM {node} n WHERE n.status = 1 AND n.created >= :startdate 
    GROUP BY month ORDER BY month ASC', array(':startdate' => $startdate))->fetchAllKeyed();
  foreach ($nodes_created_counts as $month => $count) {
    $nodes_created[$month] = $count;
  }
  foreach ($nodes_created as $month => $count) {
    $output['create history'] .= $showcount ? $count : _quickaudit_sparkline_char($count / $baseline);
  }


  $nodes_changed_counts = db_query('SELECT FROM_UNIXTIME(n.changed, \'%Y-%m\') month, COUNT(n.nid) count
    FROM {node} n WHERE n.status = 1 AND n.changed >= :startdate 
    GROUP BY month ORDER BY month ASC', array(':startdate' => $startdate))->fetchAllKeyed();
  foreach ($nodes_changed_counts as $month => $count) {
    $nodes_changed[$month] = $count;
  }
  foreach ($nodes_changed as $month => $count) {
    $output['update history'] .= $showcount ? $count : _quickaudit_sparkline_char($count / $baseline);
  }

  $output = explode("\n", _quickaudit_format_output($output, $options));
  foreach ($output as $line) {
    drush_print($line);
  }

}

function _quickaudit_format_output($input, $options = array(), $level = 0) {
  $output = array();
  
  $line_delimiter = $options['oneline'] ? ':' : "\t";
  if ($options['export']) {
    $line_delimiter = '';
  }
  
  foreach ($input as $title => $value) {
    
    // compact the title
    if ($options['export']) {
      $title = '';
    }
    else {
      if ($options['compact']) {
        $title = substr($title, 0, $level + 1);
      }
      if (!$options['oneline']) {
        $title = str_repeat('  ', $level) . $title;
      }
    }
    if (is_array($value)) {
      $output[] = $title . ($options['oneline'] ? $line_delimiter : "\n") . _quickaudit_format_output($value, $options, $level + 1);
    }
    else {
      $output[] = $title . $line_delimiter . $value;
    }
  }
  
  return implode($options['delimiter'], $output);
}

function _quickaudit_sparkline_char($percentage) {
  $add = round($percentage * 5);
  switch ($add) {
    case 0:
      return json_decode('"\u2581"');
    case 1:
      return json_decode('"\u2582"');
    case 2:
      return json_decode('"\u2583"');
    case 3:
      return json_decode('"\u2585"');
    case 4:
      return json_decode('"\u2586"');
    default:
      return json_decode('"\u2587"');
  }
}

function _quickaudit_datearray($num_months = 13) {
  $m = date('n');
  $y = date('Y');
  for ($i = 1; $i <= $num_months; $i++) {
    $datearray[$y . '-' . str_pad($m, 2, 0, STR_PAD_LEFT)] = 0;
    if ($m == 01) {
      $m = 12; $y--;
    }
    else {
      $m--;
    }
  }
  return array_reverse($datearray);
}


