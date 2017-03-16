<?php

/**
 * @file
 */

/**
 *
 */
abstract class Civimigration extends Migration {

  protected $base_table;
  protected $base_table_id;
  protected $base_table_alias;
  protected $query;
  protected $_db = '';
  protected $_db_target = 'default';
  protected $entity = 'contact';
  protected $debug = 0;
  protected $autoquery = 1;
  // If set then external_identifier will be used to look up existing contacts.
  protected $matchexternalidentifier = 1;

  /**
   *
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    $this->destination = new MigrateDestinationCivicrmApi($this->entity);
    if ($this->debug == 1) {
      $this->destination->debug = 1;
    }
    if (!empty($this->base_table) && !empty($this->base_table_id)) {
      $column = $this->gettableField($this->base_table, $this->base_table_id);
      // 'type' is returned as 'int(11)' - need to separate if there is a length.
      if (isset($column->type)) {
        $type = explode('(', $column->type);
      }
      else {
        $type = explode('(', $column->Type);
      }
      $length = array();
      if (array_key_exists(1, $type)) {
        // Length key.
        $length = array(
          'length' => substr($type[1], 0, -1),
        );
      }

      $null = isset($column->null) ? $column->null : $column->Null;

      $this->map = new MigrateSQLMap($this->machineName, array(
        $this->base_table_id => array(
          'type' => $type[0],
          'not null' => $null,
          'alias' => empty($this->base_table_alias) ? $this->base_table : $this->base_table_alias,
        ) + $length,
      ), MigrateDestinationCivicrmApi::getKeySchema());

      if ($this->autoquery == 1) {
        $query = $this->getQuery($this->base_table, $this->base_table_alias, $this->base_table_id);
        $this->source = new MigrateSourceSQL($query);
      }
    }
    $this->setDatabaseStoredMappings();
  }

  /**
   * You can override this but the default function does a lookup on external identifier
   * and does a match on that if present.
   */
  public function prepare(&$entity, &$row) {
    if ($this->matchexternalidentifier && strtolower($this->entity) == 'contact' && empty($entity->id)  && !empty($entity->external_identifier)) {
      try {
        $existingID = (civicrm_api3('contact', 'getvalue', array('version' => 3, 'return' => 'id', 'external_identifier' => $entity->external_identifier)));
        if (!empty($existingID) && !is_array($existingID)) {
          $entity->id = $existingID;
        }
      }
      catch (CiviCRM_API3_Exception $e) {
      }
    }
  }

  /**
   * Get started DB object based on table.
   */
  protected function getQuery($table, $alias, $idKey = "") {
    $query = Database::getConnection('default', $this->_db)
      ->select($table, $alias);
    $this->addFieldsFromtable($query, $table, $alias, $idKey);
    return $query;
  }

  /**
   * Add all fields to function (there is a function to do this as a * but
   * by adding them by name they become visible in the source fields in migrateUI.
   */
  protected function addFieldsFromTable(&$query, $table, $alias, $idKey = NULL) {
    $fields = $this->gettableFields($table, $alias);
    foreach ($fields as $field) {
      if (strtolower($field) == strtolower($idKey)) {
        $query->addField($alias, $field, strtolower($field));
      }
      else {
        $query->addField($alias, $field, strtolower($alias . "_" . $field));
      }
    }
    return $query;
  }

  /**
   * Get specific field in table.
   */
  protected function getTableField($table, $column) {
    $columns = Database::getConnection('default', $this->_db)
      ->query("SHOW columns from {$this->base_table} WHERE field = :column", array('column' => $column));
    foreach ($columns as $column) {
      return $column;
    }
  }

  /**
   * Get specific field in table.
   */
  protected function getMappings() {
    // Force the mapping query to be executed in the default database, as the
    // civimigrate_mappings are stored there.
    $origdb = $this->_db;
    $this->_db = 'default';

    $mappingquery = $this->getQuery('civimigrate_mappings', 'mappings');
    $mappingquery->condition('migration', $this->machineName);
    $mappings = $mappingquery->execute();

    $this->_db = $origdb;

    return $mappings;
  }

  /**
   * Get all fields in table.
   */
  protected function getTableFields($table, $alias = "") {
    $fields = array();
    $result = Database::getConnection('default', $this->_db)
      ->query("SHOW COLUMNS FROM {$table}");
    foreach ($result as $record) {
      $fields[] = $record->Field;
    }
    return $fields;
  }

  /**
   * Get all fields in table.
   */
  protected function addTableFields(&$query, $table, $alias = "") {
    if (empty($alias)) {
      $alias = $table;
    }
    $fields = array();
    $result = Database::getConnection('default', $this->_db)
      ->query("SHOW COLUMNS FROM {$table}");
    foreach ($result as $record) {
      $query->addField($alias, $record->field, $alias . "_" . strtolower($record->field));
    }
  }

  /**
   * Convert Microsoft Timestamp to mysql friendly date.
   *
   * @param string $ms_date
   *   MS date string
   * @param Bool $force
   *   Definitely needs conversion - by default function tests if it is already a valid timestamp
   *   (this is in case data is inconsistently being sent through)
   */
  function mssql2mysql($ms_date, $force = 0) {
    if (empty($ms_date)) {
      return NULL;
    }
    if (!strtotime($ms_date) || $force) {
      $timestamp = ($ms_date - 25569) * 86400;
    }
    return date('Y-m-d', $timestamp);
  }

  /**
   * Set mappings from database.
   *
   * Migrate mappings can optionally be stored in the civimigrate_mappings table.
   * This function loads those.
   */
  protected function setDatabaseStoredMappings() {
    $mappings = $this->getmappings();
    foreach ($mappings as $mapping) {
      $destField = empty($mapping->mappings_destination_field) ? NULL : $mapping->mappings_destination_field;
      $sourceField = empty($mapping->mappings_source_field) ? NULL : $mapping->mappings_source_field;
      if (empty($sourceField)) {
        $this->addFieldMapping($destField)
          ->description($mapping->mappings_description)
          ->issueGroup($mapping->mappings_issuegroup)
          ->defaultValue($mapping->mappings_default_value);
      }
      else {
        $this->addFieldMapping($destField, $sourceField)
          ->description($mapping->mappings_description)
          ->issueGroup($mapping->mappings_issuegroup)
          ->defaultValue($mapping->mappings_default);
      }
    }
  }

}
