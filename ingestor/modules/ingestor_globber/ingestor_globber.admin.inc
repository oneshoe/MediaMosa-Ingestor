<?php

/**
 * @file
 * Administration and configuration functions of ingestor module.
 */

/**
 * Settings form for configuring search paths for files.
 */
function ingestor_globber_settings_form() {
  $form = array();

  $form['ingestor_globber_dirs'] = array(
    '#type' => 'textarea',
    '#title' => t('Included paths'),
    '#description' => t('Include these paths in the search. One path per line.'),
    '#element_validate' => array('ingestor_globber_validate_paths'),
    '#default_value' => implode("\n", variable_get('ingestor_globber_dirs', array())),
  );

  return system_settings_form($form);
}

/**
 * Replace the textarea string with an array.
 */
function ingestor_globber_validate_paths($element, &$form_state, $form) {
  $value = array_map('trim', array_filter(preg_split( '/\r\n|\r|\n/', trim($element['#value']))));
  $form_state['values']['ingestor_globber_dirs'] = $value;
}
