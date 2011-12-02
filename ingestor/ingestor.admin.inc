<?php
// $Id: ingestor.admin.inc 243 2011-11-24 23:56:27Z thijs $

/**
 * @file
 * Administration and configuration functions of ingestor module.
 */

/**
 * Settings form for configuring MediaMosa processing after upload.
 */
function ingestor_settings_form() {
  $form = array();

  $form['ingestor'] = array(
    '#type' => 'fieldset',
    '#title' => t('General'),
    '#collapsible' => TRUE,
  );
  $form['ingestor']['ingestor_user_id'] = array(
    '#type' => 'textfield',
    '#title' => t('MediaMosa user id.'),
    '#size' => 29,
    '#default_value' => variable_get('ingestor_user_id', 'ingestor'),
  );
  $form['ingestor']['ingestor_work_dir'] = array(
    '#type' => 'textfield',
    '#title' => t('Working directory'),
    '#size' => 29,
    '#element_validate' => array('ingestor_validate_work_dir'),
    '#default_value' => variable_get('ingestor_work_dir', Ingestor::DEFAULT_WORK_DIR),
  );

  $form['ingestors'] = array(
    '#type' => 'fieldset',
    '#title' => t('Ingestors'),
    '#collapsible' => TRUE,
  );

  // Get a list of ingestor types.
  $ingestor_types = module_invoke_all('ingestor_supported_types');

  if (empty($ingestor_types)) {
    $form['ingestors']['#description'] = t('There are no Ingestor handler modules enabled. Please enable Ingestor handler modules from the <a href="@link_modules">Modules page</a>.', array(
      '@link_modules' => url('admin/modules'),
    ));
  }
  else {
    // Get a list of transcoding profiles
    $profiles = Ingestor::get_profiles();

    foreach ($ingestor_types as $name => $info) {
      $form['ingestors'][$name] = array(
        '#type' => 'fieldset',
        '#title' => filter_xss($info['title']),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );

      $form['ingestors'][$name]['ingestor_' . $info['class'] . '_profiles'] = array(
        '#type' => 'checkboxes',
        '#title' => t('Transcodings'),
        '#description' => t('Specify to which media formats the ingested presentations should be converted after they have been uploaded to MediaMosa.'),
        '#options' => $profiles,
        '#default_value' => variable_get('ingestor_' . $info['class'] . '_profiles', array()),
      );
    }
  }

  return system_settings_form($form);
}

/**
 * Validation function for checking if the supplied working directory exists
 * and is writable.
 */
function ingestor_validate_work_dir($element, &$form_state, $form) {
  if (!file_prepare_directory($element['#value'])) {
    form_error($element, t('Working directory does not exist or is not writable.'));
  }
}
