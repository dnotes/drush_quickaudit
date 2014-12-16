<?php

function quickaudit_drush_command() {
  $items = array();
  $items['quickaudit-content'] = array(
    'description' => "Quick audit of a site's content",
    'options' => array(
      'showcount' => "Show the count of nodes created and updated instead of graphic"
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

  $total_nodes = db_query("SELECT COUNT(*) FROM {node} WHERE status")->fetchField();
  if (!$total_nodes) {
    drush_print('No nodes found');
    return;
  }
  
  $show_count = drush_get_option('showcount');

  $then = new dateTime();
  $then->setTimestamp(db_query("SELECT min(created) FROM {node} WHERE created")->fetchField());
  $now = new dateTime();
  $now->setTimestamp(time());
  $interval = $then->diff($now);
  $site_longevity = ($interval->y * 12) + $interval->m + 1;
  $npm = round($total_nodes / $site_longevity);
  
  $nodes_by_type = db_query("SELECT type, COUNT(nid) count  FROM {node} WHERE status GROUP BY type ORDER BY count DESC")->fetchAllKeyed();
  $x = 1;
  $type_output = array();
  foreach ($nodes_by_type as $type => $count) {
    $pct = number_format(($count / $total_nodes) * 100);
    if ($x < 3 || $pct > 30) {
      $type_output[] = substr($type, 0, 2) . ':' . $pct . '%';
      $x++;
    }
    else {
      continue;
    }
  }
  $type_output = implode($type_output, ',');
  
  $nodes_by_user = db_query("SELECT u.name, COUNT(n.nid) count FROM {node} n LEFT JOIN {users} u ON n.uid = u.uid WHERE n.status GROUP BY u.name ORDER BY count DESC LIMIT 0,5")->fetchAllKeyed();
  $x = 1;
  $authors = array();
  foreach ($nodes_by_user as $name => $count) {
    if ($x == 1) {
      $authors[$name] = $count;
      $authors['u'] = $count;
    }
    else {
      $authors['g'] += $count;
    }
    $x++;
  }
  $authors['o'] = $total_nodes - ($authors['u'] + $authors['g']);
  unset($authors['u']);
  foreach ($authors as $name => $count) {
    $authors_output[] = $name . ':' . number_format(($count / $total_nodes) * 100) . '%';
  }
  $authors_output = implode(',', $authors_output);



  $curmonth = date('n');
  $lastyear = date('Y', strtotime('-1 year'));
  $startdate = mktime(0,0,0,$curmonth,1,$lastyear);

  $num_months = min($site_longevity + 1, 13);
  
  $baseline = $npm < 10 ? $npm * 3 : $npm * 2;

  $created_sparkline = $changed_sparkline = '';
  $nodes_created = $nodes_changed = _quickaudit_datearray($num_months);

  $nodes_created_counts = db_query('SELECT FROM_UNIXTIME(n.created, \'%Y-%m\') month, COUNT(n.nid) count
    FROM {node} n WHERE n.status = 1 AND n.created >= :startdate 
    GROUP BY month ORDER BY month ASC', array(':startdate' => $startdate))->fetchAllKeyed();
  foreach ($nodes_created_counts as $month => $count) {
    $nodes_created[$month] = $count;
  }
  foreach ($nodes_created as $month => $count) {
    $created_sparkline .= $show_count ? $count : _quickaudit_sparkline_char($count / $baseline);
  }


  $nodes_changed_counts = db_query('SELECT FROM_UNIXTIME(n.changed, \'%Y-%m\') month, COUNT(n.nid) count
    FROM {node} n WHERE n.status = 1 AND n.changed >= :startdate 
    GROUP BY month ORDER BY month ASC', array(':startdate' => $startdate))->fetchAllKeyed();
  foreach ($nodes_changed_counts as $month => $count) {
    $nodes_changed[$month] = $count;
  }
  foreach ($nodes_changed as $month => $count) {
    $changed_sparkline .= $show_count ? $count : _quickaudit_sparkline_char($count / $baseline);
  }

  global $base_url;

  $output = "$base_url | $total_nodes | $type_output | $authors_output | ${npm}npm $months C:$created_sparkline U:$changed_sparkline";
  drush_print($output);

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


