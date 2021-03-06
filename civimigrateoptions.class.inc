<?php

/**
 * @file
 */

/**
 *
 */
class CiviMigrateOptions extends Civimigration {
  protected $debug = 0;
  // Set to 1 for debug info.
  protected $json_folder = NULL;
  protected $options = array();
  protected $entity = 'option_group';

  /**
   *
   */
  public function __construct($arguments = array()) {
    parent::__construct($arguments);
    $this->description = t('Create custom values.');

    // This can also be an URL instead of a file path but we are assuming it shipe with the migrate module.
    $json_folder = $this->json_folder;
    if (empty($json_folder)) {
      throw new MigrateException('json folder must be defined');
    }
    $list_url = $json_folder . 'list.json';
    $item_url = $json_folder . '/optionvalues.json';
    $http_options = array();
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'fieldname' => array(
          'type' => 'varchar',
          'length' => 20,
          'not null' => TRUE,
        ),
      ),
      MigrateDestinationCivicrmApi::getKeySchema()
    );
    $this->source = new MigrateSourceList(
      new CiviMigrateListJSON($item_url, 'name'),
      new CiviMigrateFileItemJSON($item_url, $http_options, 'name'),
      $this->fields()
    );
    $this->addFieldMapping('name', 'name');
    $this->addFieldMapping('label', 'label');
    $this->addFieldMapping('value', 'value');
    $this->addFieldMapping('is_active')->defaultValue(1);
    $this->addFieldMapping('description', 'description')->defaultValue('String');
  }

  /**
   * Return the fields (this is cleaner than passing in the array in the MigrateSourceList class above).
   *
   * @return array
   */
  function fields() {
    return array(
      'title' => 'The title of the content',
      'name' => t('Option Group Name'),
      'title' => t('title'),
      'label' => t('value'),
      'data_type' => t('description'),
    );
  }

  /**
   * @see Civimigration::prepare()
   */
  function prepare(&$entity, &$row) {
    if (empty($entity->id)) {
      $entity->id = $this->getOptionGroupID($row->name);
    }
    foreach ($row->options as $option) {
      $this->ensureOptionValueID($option, $entity);
    }
  }

  /**
   * Get or create relevant group id.
   *
   * @param object $entity
   * @param stdClass $row
   *    * function complete($entity, $row){
   *    * }   *    */
  /**
   * Here we establish all our custom fields
   * (non-PHPdoc).
   *
   * @see Migration::preImport()
   *    * public function postImport() {
   * parent::postImport();
   * if(!civicrm_initialize()){
   * return;
   * }
   * // need to clear the cache once the fields are created
   * civicrm_api3('system', 'flush', array('version' => 3));
   * }
   */

  /**
   * Get or create relevant group id.
   *
   * @param int $groupTitle
   * @param string $extends
   *
   * @throws MigrateException
   *
   * @return integer groupID
   */
  function getOptionGroupID($groupTitle) {
    $group = civicrm_api3('option_group', 'get', array(
        'version' => 3,
        'name' => $groupTitle,
        'return' => 'id',
      )
    );

    if (!empty($group['id'])) {
      // Make sure it's active.
      civicrm_api3('option_group', 'create', array(
        'version' => 3,
        'id' => $group['id'],
        'is_active' => 1,
      ));
      return $group['id'];
    }
    $group = civicrm_api3('option_group', 'create', array(
        'version' => 3,
        'name' => $groupTitle,
        'is_active' => 1,
      )
    );
    if ($group['is_error']) {
      throw new MigrateException("failed to create $groupTitle  : " . $group['error_message']);
    }
    return $group['id'];
  }

  /**
   * Get / create custom field.
   *
   * @param int $customGroupID
   * @param string $field
   *   - this will be the key of a field in $this->customFields
   */
  function ensureOptionValueID($option, $entity) {
    $params = array(
      'version' => 3,
      'option_group_id' => $entity->id,
    );
    foreach ($option as $property => $value) {
      $params[$property] = $value;
    }

    $optionValue = civicrm_api3('option_value', 'get', $params);

    if (empty($optionValue['id'])) {
      $result = civicrm_api3('option_value', 'create', $params);
      drupal_set_message("<pre>" . print_r($result, 1) . "</pre>");
      if ($result['is_error']) {
        throw new MigrateException('failed to create value ' . print_r($result, 1));
      }
    }

    return NULL;
  }

  /**
   * Get required options set.
   *
   * @param string $field
   *   - this will be the key of a field in $this->customFields
   */
  function getFieldOptions($table, $keyField, $labelField) {
    $query = db_select($table, 'tbl');
    $query->addField('tbl', $keyField, 'key_field');
    $query->addField('tbl', $labelField, 'label');
    return $query->execute()->fetchAllKeyed();
  }

}
