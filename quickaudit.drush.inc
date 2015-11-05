<?php

function quickaudit_drush_command() {
  $items = array();
  $items['quickaudit-content'] = array(
    'description' => "Quick audit of a site's content",
    'options' => array(
      'oneline' => "Place all results on one line.",
      'no-titles' => "Do not show titles.",
      'compact' => "Compact titles. A number of characters may be specified.",
      'fill-fields' => "Ensure all possible fields are filled.",
      'raw-percentages' => "Show raw percentages where available.",
      'raw-counts' => "Show raw counts where available. Incompatible with showpcts.",
      'sparklines' => "Show sparklines where available, overriding raw options above.",
      'compact-sparklines' => "Do not add spaces to sparklines. Good with --export --sparklines.",
      'delimiter' => "Set the delimiter between fields. Defaults to tab for export, line break for standard.",
      'export' => "Prepare data for export to spreadsheet: no-titles, oneline, fill-fields, raw outputs, tab delimited.",
      'months' => "Number of months to query. Defaults to 12.",
      'types-lifetime' => "Show lifetime content types for comparison. Defaults to true unless oneline.",
      'sponsor-emails' => "Include sponsor email.",
      'admin-emails' => "Include admin emails",
    ),
    'examples' => array(
      'drush qa-content' => "A page of data about a site's content",
      'drush qa-content --months=6' => "Page of data going back to the beginning of last " . date('F', strtotime("-6 months")),
      'drush qa-content --oneline --compact' => "Data on a single line with single letter titles",
      'drush qa-content --export' => "Data on a single line with no titles, ready for export to spreadsheet",
      'drush qa-content --export --sparklines' => "Data ready for export, but with sparklines where applicable",
    ),
    'aliases' => array('qa-content'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  $items['quickaudit-users'] = array(
    'description' => "Quick audit of a site's users",
    'options' => array(
      
      'no-titles' => "Do not show titles.",
      'email-list' => "One line, formatted list of names and email addresses only.",
      'roles' => 'Specify a comma-separated list of roles, e.g. --role="administrator,editor".',
      'include-u1' => "Include user 1.",
      'raw-usernames' => "Use raw usernames instead of realname field.",
      'include-sitename' => "Include site name in results.",
      
    ),
    'examples' => array(
      
    ),
    'aliases' => array('qa-users'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  return $items;
}

function drush_quickaudit_users($return = FALSE) {
  $options = array();
  $options['email-list'] = $email_list = drush_get_option('email-list', FALSE);
  if ($roles = drush_get_option('roles')) {
    $options['roles'] = $roles = array_map('trim', explode(',', $roles));
  }
  $options['no-titles'] = $no_titles = drush_get_option('no-titles', FALSE);
  $options['include-u1'] = $include_u1 = drush_get_option('include-u1', FALSE);
  $options['raw-usernames'] = $raw_usernames = drush_get_option('raw-usernames', FALSE);
  $options['include-sitename'] = $include_sitename = drush_get_option('include-sitename', FALSE);
  
  $query = db_select('users', 'u');
  $query->fields('u', array('uid', 'name', 'mail', 'created', 'access', 'login', 'status'));
  $query->condition('u.status', 1);

  if (!$raw_usernames && !$email_list && module_exists('realname')) {
    $query->join('field_data_field_realname', 'f', 'f.entity_id = u.uid');
    $query->addField('f', 'field_realname_value', 'realname');
  }

  if (!empty($roles)) {
    $query->distinct();
    $query->join('users_roles', 'ur', 'ur.uid = u.uid');
    $query->join('role', 'r', 'r.rid = ur.rid');
    $query->condition('r.name', $roles);
  }

  if (!$include_u1) {
    $query->condition('u.uid', 1, '!=');
  }

  $users = $query->execute()->fetchAllAssoc('uid');
  
  if (empty($users)) {
    return;
  }
  
  global $base_url;

  if ($email_list) {
    $emails = array();
    foreach ($users as $u) {
      $emails[] = $u->mail;
    }
    $output = array('emails' => implode(', ', $emails));
    $options['oneline'] = $options['no-titles'] = $options['compact'] = $options['export'] = TRUE;
  }
  else {
    if (!$include_sitename) {
      $output['site'] = $base_url;
    }
    if (!$no_titles) {
      $output['titles'] = array(t('Name'), t('Email'), t('Created'), t('Last access'), t('Last login'));
      if ($include_sitename) {
        $output['titles'][] = t('Site');
      }
    }
    foreach ($users as $u) {
      $output["user: $u->name"] = array(
        'name' => !empty($u->realname) ? $u->realname : $u->name,
        'mail' => $u->mail,
        'created' => $u->created ? format_date($u->created, 'custom', 'Y-m-d') : t('never'),
        'access' => $u->access ? format_date($u->access, 'custom', 'Y-m-d') : t('never'),
        'login' => $u->login ? format_date($u->login, 'custom', 'Y-m-d') : t('never'),
      );
      if ($include_sitename) {
        $output["user: $u->name"]['site'] = $base_url;
      }
    }
    $options['no-titles'] = $options['compact'] = TRUE;
  }
  
  if ($return) {
    return _quickaudit_format_output($output, $options);
  }
  drush_print(_quickaudit_format_output($output, $options));
}

function drush_quickaudit_content() {

  // Find total nodes, in case there is nothing.
  $all_published_nodes = db_query("SELECT COUNT(*) FROM {node} WHERE status")->fetchField();
  if (!$all_published_nodes) {
    drush_print('No nodes found.');
    return;
  }
  $all_published_nodes_including_unpublished = db_query("SELECT COUNT(*) FROM {node}")->fetchField();
  
  $options = array();
  $options['export'] = $export = drush_get_option('export');
  $options['oneline'] = $oneline = drush_get_option('oneline', !empty($export));
  $options['no-titles'] = $no_titles = drush_get_option('no-titles', !empty($export));
  $options['compact'] = $compact = drush_get_option('compact', !empty($export));
  $options['fill-fields'] = $fill_fields = drush_get_option('fill-fields', !empty($export));
  $options['raw-percentages'] = $raw_percentages = drush_get_option('raw-percentages', !empty($export));
  $options['raw-counts'] = $raw_counts = drush_get_option('raw-counts', !empty($export));
  $options['sparklines'] = $sparklines = drush_get_option('sparklines', !($raw_counts || $raw_percentages));
  $options['compact-sparklines'] = drush_get_option('compact-sparklines');
  $delimiter_default = $export ? "\t" : ($oneline ? " | " : "\n");
  $options['delimiter'] = $delimiter = drush_get_option('delimiter', $delimiter_default);
  $options['months'] = $months = drush_get_option('months', 12);
  $options['types-lifetime'] = drush_get_option('types-lifetime', empty($oneline));
  
  // Find the starting date
  $starttime = strtotime("-$months months");
  $calc_month = date('n', $starttime);
  $calc_year = date('Y', $starttime);
  $startdate = mktime(0,0,0,$calc_month,1,$calc_year);

  // Find the total number of nodes within the time period
  $total_nodes = db_query("SELECT COUNT(*) FROM {node} WHERE status AND created >= :created", array(':created' => $startdate))->fetchField();
  
  // Output the URL
  global $base_url;
  $output['url'] = preg_replace('|^.*://|', '', $base_url);
  

  // Calculate site longevity by comparing the creation date of the first user to time().
  $then = new dateTime();
  $then->setTimestamp(db_query("SELECT min(created) FROM {users} WHERE created")->fetchField());
  $now = new dateTime();
  $now->setTimestamp(time());
  $interval = $then->diff($now);
  $site_longevity = ($interval->y * 12) + $interval->m + 1;
  $months_for_calculation = min($months, $site_longevity);
  
  // Get status
  if (module_exists('incubator')) {
    $output['status'] = variable_get('incubator_disable_site', TRUE) ? 'disabled' : 'enabled';
  }
  
  // Get months
  $output['lifetime'] = array(
    'months' => ($compact ? $site_longevity : "{$site_longevity}mo"),
    'nodes' => $all_published_nodes,
    'npm' => round($all_published_nodes / $site_longevity, 1),
  );
  
  if ($options['types-lifetime']) {
    // ===========================================================================
    // Node types
    $nodes_by_type = db_query("SELECT type, COUNT(nid) count FROM {node} "
        . "WHERE status GROUP BY type ORDER BY count DESC LIMIT 3")->fetchAllKeyed();
    foreach ($nodes_by_type as $type => $count) {
      $pct = round($count / $all_published_nodes, 2);
      if ($pct > .30 || ($fill_fields && count($output['lifetime']['types']) < 3)) {
        $output['lifetime']['types'][$type] = $raw_percentages ? $pct : ($raw_counts ? $count : $pct * 100 . '%');
      }
    }
    $type_names = implode(',', array_keys($output['lifetime']['types']));
    if ($fill_fields) {
      while (count($output['lifetime']['types']) < 3) {
        $output['lifetime']['types'][] = "0.00";
      }
    }
    if ($no_titles) {
      $output['lifetime']['types']['types'] = $type_names;
    }
  }
  
  
  // Get months, nodes, and npm
  $output['months'] = $months_for_calculation;
  $output['nodes'] = $total_nodes;
  $npm = round($total_nodes / $months_for_calculation, 1);
  $output['npm'] = $export ? $npm : "{$npm}/m";
  
  // ===========================================================================
  // Node types
  $nodes_by_type = db_query("SELECT type, COUNT(nid) count FROM {node} "
      . "WHERE status AND created >= :created GROUP BY type ORDER BY count DESC LIMIT 3", array(':created' => $startdate))->fetchAllKeyed();
  foreach ($nodes_by_type as $type => $count) {
    $pct = round($count / $total_nodes, 2);
    if ($pct > .30 || ($fill_fields && count($output['types']) < 3)) {
      $output['types'][$type] = $raw_percentages ? $pct : ($raw_counts ? $count : $pct * 100 . '%');
    }
  }
  $type_names = implode(',', array_keys($output['types']));
  if ($fill_fields) {
    while (count($output['types']) < 3) {
      $output['types'][] = "0.00";
    }
  }
  if ($no_titles) {
    $output['types']['types'] = $type_names;
  }
  
  // ===========================================================================
  // Authorship
  if (empty($total_nodes)) {
    $output['authors'] = $options['fill-fields'] ? array('user' => 0, 'group' => 0, 'other' => 0) : 0;
  }
  else {
    $nodes_by_user = db_query("SELECT u.name, COUNT(n.nid) count FROM {node} n LEFT JOIN {users} u ON n.uid = u.uid "
        . "WHERE n.status AND n.created >= :created GROUP BY u.name ORDER BY count DESC LIMIT 0,5", array(':created' => $startdate))->fetchAllKeyed();
    $x = 1;
    foreach ($nodes_by_user as $name => $count) {
      if ($x == 1) {
        $user_name = $compact ? 'user' : $name;
        $authors[$user_name] = $count;
        $authors['group'] = 0;
        $authors['other'] = 0;
      }
      else {
        $authors['group'] += $count;
      }
      $x++;
    }
    $authors['other'] = $total_nodes - ($authors[$user_name] + $authors['group']);

    if (empty($authors['group'])) {
      $authors['group'] = 0;
    }
    foreach ($authors as $name => $count) {
      $pct = round($count / $total_nodes, 2);
      $name = $compact ? substr($name, 0, 1) : $name;
      $output['authors'][$name] = $raw_percentages ? $pct : ($raw_counts ? $count : $pct * 100 . '%');
    }
  }

  
  // Set the baseline for sparklines, so that content can be evaluated compared to npm.
  $baseline = $npm < 6 ? $npm * 3 : $npm * 2;
  if ($baseline < 8) {
    $baseline = 8;
  }

  $output['update history'] = $output['create history'] = '';
  $fill_months = ($options['fill-fields'] && !$options['sparklines']) ? $months : $months_for_calculation;
  $nodes_created = $nodes_updated = _quickaudit_datearray($fill_months + 1);

  $nodes_created_counts = db_query('SELECT FROM_UNIXTIME(n.created, \'%Y-%m\') month, COUNT(n.nid) count
    FROM {node} n WHERE n.status = 1 AND n.created >= :startdate 
    GROUP BY month ORDER BY month ASC', array(':startdate' => $startdate))->fetchAllKeyed();
  foreach ($nodes_created_counts as $month => $count) {
    $nodes_created[$month] = $count;
  }
  $output['create history'] = _quickaudit_sparkline($nodes_created, $baseline, $options);


  $nodes_updated_counts = db_query('SELECT FROM_UNIXTIME(n.changed, \'%Y-%m\') month, COUNT(n.nid) count
    FROM {node} n WHERE n.status = 1 AND n.changed >= :startdate 
    GROUP BY month ORDER BY month ASC', array(':startdate' => $startdate))->fetchAllKeyed();
  foreach ($nodes_updated_counts as $month => $count) {
    $nodes_updated[$month] = $count;
  }
  $output['update history'] = _quickaudit_sparkline($nodes_updated, $baseline, $options);
  
  if (drush_get_option('sponsor-emails', FALSE)) {
    $output['sponsor email'] = variable_get('incubator_us_sponsor_email', '');
  }
  if (drush_get_option('admin-emails', FALSE)) {
    drush_set_option('email-list', TRUE);
    drush_set_option('roles', 'superadmin,administrator');
    $output['admin emails'] = drush_quickaudit_users(TRUE);
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
  
  if ($options['compact']) {
    $chars = (is_numeric($options['compact']) && $options['compact'] > 1) ? $options['compact'] : $level + 1;
  }
  
  foreach ($input as $title => $value) {
    
    // format the title
    if ($options['no-titles']) {
      $title = '';
    }
    else {
      if ($options['compact']) {
        $title = substr($title, 0, $chars);
      }
      if (!$options['oneline']) {
        $title = str_repeat('  ', $level) . $title;
      }
    }
    
    // handle arrays
    if (is_array($value)) {
      if ($options['export']) {
        $output[] = _quickaudit_format_output($value, $options, $level + 1);
      }
      elseif ($options['oneline']) {
        $output[] = $title . '{' . _quickaudit_format_output($value, $options, $level + 1) . '}';
      }
      else {
        $output[] = $title . "\n" . _quickaudit_format_output($value, $options, $level + 1);
      }
    }
    
    // handle single values
    else {
      $output[] = $title . $line_delimiter . $value;
    }
  }
  
  return implode($options['delimiter'], $output);
}

function _quickaudit_sparkline($values, $baseline, $options) {
  foreach ($values as $title => $value) {
    if ($options['sparklines']) {
      $output[] = _quickaudit_sparkline_char($value / $baseline);
    }
    elseif ($options['raw-counts']) {
      $output[] = $value;
    }
    elseif ($options['raw-percentages']) {
      $output[] = $value / $baseline;
    }
    else {
      $output[$title] = $value;
    }
  }
  if (!($options['sparklines'] || $options['raw-counts'] || $options['raw-percentages'])) {
    return $output;
  }
  $glue = $options['sparklines'] ? ($options['compact-sparklines'] ? '' : ' ') : ($options['export'] ? "\t" : ' ');
  return implode($glue, $output);
}

function _quickaudit_sparkline_char($percentage) {
  static $chars;
  if (!isset($chars)) {
    $chars = array(
      json_decode('"\u2581"'),
      json_decode('"\u2582"'),
      json_decode('"\u2583"'),
      json_decode('"\u2585"'),
      json_decode('"\u2586"'),
      json_decode('"\u2587"'),
    );
  }
  $add = round($percentage * 5);
  if ($add > 5) {
    $add = 5;
  }
  return $chars[$add];
}

function _quickaudit_datearray($num_months) {
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


