// Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"), you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//
// Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
// Portions Copyright (c) 1994, Regents of the University of California
// Portions Copyright (c) 2021 Futurewei Cloud

#pragma once

#include <stdint.h>

#include <string>

/* These macro definitions have been taken from elog.h */
#define K2PG_PGSIXBIT(ch) (((ch) - '0') & 0x3F)

#define K2PG_MAKE_SQLSTATE(ch1, ch2, ch3, ch4, ch5) \
    (K2PG_PGSIXBIT(ch1) + (K2PG_PGSIXBIT(ch2) << 6) + (K2PG_PGSIXBIT(ch3) << 12) + \
    (K2PG_PGSIXBIT(ch4) << 18) + (K2PG_PGSIXBIT(ch5) << 24))

/* The actual PostgreSQL error codes have been taken from the generated errcodes.h file. */
namespace k2pg {

enum class K2PgErrorCode : uint32_t {
  /* Class 00 - Successful Completion */
  K2PG_SUCCESSFUL_COMPLETION = K2PG_MAKE_SQLSTATE('0', '0', '0', '0', '0'),

  /* Class 01 - Warning */
  K2PG_WARNING = K2PG_MAKE_SQLSTATE('0', '1', '0', '0', '0'),
  K2PG_WARNING_DYNAMIC_RESULT_SETS_RETURNED = K2PG_MAKE_SQLSTATE('0', '1', '0', '0', 'C'),
  K2PG_WARNING_IMPLICIT_ZERO_BIT_PADDING = K2PG_MAKE_SQLSTATE('0', '1', '0', '0', '8'),
  K2PG_WARNING_NULL_VALUE_ELIMINATED_IN_SET_FUNCTION =
      K2PG_MAKE_SQLSTATE('0', '1', '0', '0', '3'),
  K2PG_WARNING_PRIVILEGE_NOT_GRANTED = K2PG_MAKE_SQLSTATE('0', '1', '0', '0', '7'),
  K2PG_WARNING_PRIVILEGE_NOT_REVOKED = K2PG_MAKE_SQLSTATE('0', '1', '0', '0', '6'),
  K2PG_WARNING_STRING_DATA_RIGHT_TRUNCATION = K2PG_MAKE_SQLSTATE('0', '1', '0', '0', '4'),
  K2PG_WARNING_DEPRECATED_FEATURE = K2PG_MAKE_SQLSTATE('0', '1', 'P', '0', '1'),

  /* Class 02 - No Data (this is also a warning class per the SQL standard) */
  K2PG_NO_DATA = K2PG_MAKE_SQLSTATE('0', '2', '0', '0', '0'),
  K2PG_NO_ADDITIONAL_DYNAMIC_RESULT_SETS_RETURNED = K2PG_MAKE_SQLSTATE('0', '2', '0', '0', '1'),

  /* Class 03 - SQL Statement Not Yet Complete */
  K2PG_SQL_STATEMENT_NOT_YET_COMPLETE = K2PG_MAKE_SQLSTATE('0', '3', '0', '0', '0'),

  /* Class 08 - Connection Exception */
  K2PG_CONNECTION_EXCEPTION = K2PG_MAKE_SQLSTATE('0', '8', '0', '0', '0'),
  K2PG_CONNECTION_DOES_NOT_EXIST = K2PG_MAKE_SQLSTATE('0', '8', '0', '0', '3'),
  K2PG_CONNECTION_FAILURE = K2PG_MAKE_SQLSTATE('0', '8', '0', '0', '6'),
  K2PG_SQLCLIENT_UNABLE_TO_ESTABLISH_SQLCONNECTION =
      K2PG_MAKE_SQLSTATE('0', '8', '0', '0', '1'),
  K2PG_SQLSERVER_REJECTED_ESTABLISHMENT_OF_SQLCONNECTION =
      K2PG_MAKE_SQLSTATE('0', '8', '0', '0', '4'),
  K2PG_TRANSACTION_RESOLUTION_UNKNOWN = K2PG_MAKE_SQLSTATE('0', '8', '0', '0', '7'),
  K2PG_PROTOCOL_VIOLATION = K2PG_MAKE_SQLSTATE('0', '8', 'P', '0', '1'),

  /* Class 09 - Triggered Action Exception */
  K2PG_TRIGGERED_ACTION_EXCEPTION = K2PG_MAKE_SQLSTATE('0', '9', '0', '0', '0'),

  /* Class 0A - Feature Not Supported */
  K2PG_FEATURE_NOT_SUPPORTED = K2PG_MAKE_SQLSTATE('0', 'A', '0', '0', '0'),

  /* Class 0B - Invalid Transaction Initiation */
  K2PG_INVALID_TRANSACTION_INITIATION = K2PG_MAKE_SQLSTATE('0', 'B', '0', '0', '0'),

  /* Class 0F - Locator Exception */
  K2PG_LOCATOR_EXCEPTION = K2PG_MAKE_SQLSTATE('0', 'F', '0', '0', '0'),
  K2PG_L_E_INVALID_SPECIFICATION = K2PG_MAKE_SQLSTATE('0', 'F', '0', '0', '1'),

  /* Class 0L - Invalid Grantor */
  K2PG_INVALID_GRANTOR = K2PG_MAKE_SQLSTATE('0', 'L', '0', '0', '0'),
  K2PG_INVALID_GRANT_OPERATION = K2PG_MAKE_SQLSTATE('0', 'L', 'P', '0', '1'),

  /* Class 0P - Invalid Role Specification */
  K2PG_INVALID_ROLE_SPECIFICATION = K2PG_MAKE_SQLSTATE('0', 'P', '0', '0', '0'),

  /* Class 0Z - Diagnostics Exception */
  K2PG_DIAGNOSTICS_EXCEPTION = K2PG_MAKE_SQLSTATE('0', 'Z', '0', '0', '0'),
  K2PG_STACKED_DIAGNOSTICS_ACCESSED_WITHOUT_ACTIVE_HANDLER =
      K2PG_MAKE_SQLSTATE('0', 'Z', '0', '0', '2'),

  /* Class 20 - Case Not Found */
  K2PG_CASE_NOT_FOUND = K2PG_MAKE_SQLSTATE('2', '0', '0', '0', '0'),

  /* Class 21 - Cardinality Violation */
  K2PG_CARDINALITY_VIOLATION = K2PG_MAKE_SQLSTATE('2', '1', '0', '0', '0'),

  /* Class 22 - Data Exception */
  K2PG_DATA_EXCEPTION = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '0'),
  K2PG_ARRAY_ELEMENT_ERROR = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', 'E'),
  K2PG_ARRAY_SUBSCRIPT_ERROR = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', 'E'),
  K2PG_CHARACTER_NOT_IN_REPERTOIRE = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', '1'),
  K2PG_DATETIME_FIELD_OVERFLOW = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '8'),
  K2PG_DATETIME_VALUE_OUT_OF_RANGE = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '8'),
  K2PG_DIVISION_BY_ZERO = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '2'),
  K2PG_ERROR_IN_ASSIGNMENT = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '5'),
  K2PG_ESCAPE_CHARACTER_CONFLICT = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'B'),
  K2PG_INDICATOR_OVERFLOW = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', '2'),
  K2PG_INTERVAL_FIELD_OVERFLOW = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '5'),
  K2PG_INVALID_ARGUMENT_FOR_LOG = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', 'E'),
  K2PG_INVALID_ARGUMENT_FOR_NTILE = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '4'),
  K2PG_INVALID_ARGUMENT_FOR_NTH_VALUE = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '6'),
  K2PG_INVALID_ARGUMENT_FOR_POWER_FUNCTION = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', 'F'),
  K2PG_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', 'G'),
  K2PG_INVALID_CHARACTER_VALUE_FOR_CAST = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '8'),
  K2PG_INVALID_DATETIME_FORMAT = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '7'),
  K2PG_INVALID_ESCAPE_CHARACTER = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '9'),
  K2PG_INVALID_ESCAPE_OCTET = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'D'),
  K2PG_INVALID_ESCAPE_SEQUENCE = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', '5'),
  K2PG_NONSTANDARD_USE_OF_ESCAPE_CHARACTER = K2PG_MAKE_SQLSTATE('2', '2', 'P', '0', '6'),
  K2PG_INVALID_INDICATOR_PARAMETER_VALUE = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '0'),
  K2PG_INVALID_PARAMETER_VALUE = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', '3'),
  K2PG_INVALID_PRECEDING_OR_FOLLOWING_SIZE = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '3'),
  K2PG_INVALID_REGULAR_EXPRESSION = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', 'B'),
  K2PG_INVALID_ROW_COUNT_IN_LIMIT_CLAUSE = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', 'W'),
  K2PG_INVALID_ROW_COUNT_IN_RESULT_OFFSET_CLAUSE = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', 'X'),
  K2PG_INVALID_TABLESAMPLE_ARGUMENT = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', 'H'),
  K2PG_INVALID_TABLESAMPLE_REPEAT = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', 'G'),
  K2PG_INVALID_TIME_ZONE_DISPLACEMENT_VALUE = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '9'),
  K2PG_INVALID_USE_OF_ESCAPE_CHARACTER = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'C'),
  K2PG_MOST_SPECIFIC_TYPE_MISMATCH = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'G'),
  K2PG_NULL_VALUE_NOT_ALLOWED = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '4'),
  K2PG_NULL_VALUE_NO_INDICATOR_PARAMETER = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '2'),
  K2PG_NUMERIC_VALUE_OUT_OF_RANGE = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '3'),
  K2PG_SEQUENCE_GENERATOR_LIMIT_EXCEEDED = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'H'),
  K2PG_STRING_DATA_LENGTH_MISMATCH = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', '6'),
  K2PG_STRING_DATA_RIGHT_TRUNCATION = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', '1'),
  K2PG_SUBSTRING_ERROR = K2PG_MAKE_SQLSTATE('2', '2', '0', '1', '1'),
  K2PG_TRIM_ERROR = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', '7'),
  K2PG_UNTERMINATED_C_STRING = K2PG_MAKE_SQLSTATE('2', '2', '0', '2', '4'),
  K2PG_ZERO_LENGTH_CHARACTER_STRING = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'F'),
  K2PG_FLOATING_POINT_EXCEPTION = K2PG_MAKE_SQLSTATE('2', '2', 'P', '0', '1'),
  K2PG_INVALID_TEXT_REPRESENTATION = K2PG_MAKE_SQLSTATE('2', '2', 'P', '0', '2'),
  K2PG_INVALID_BINARY_REPRESENTATION = K2PG_MAKE_SQLSTATE('2', '2', 'P', '0', '3'),
  K2PG_BAD_COPY_FILE_FORMAT = K2PG_MAKE_SQLSTATE('2', '2', 'P', '0', '4'),
  K2PG_UNTRANSLATABLE_CHARACTER = K2PG_MAKE_SQLSTATE('2', '2', 'P', '0', '5'),
  K2PG_NOT_AN_XML_DOCUMENT = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'L'),
  K2PG_INVALID_XML_DOCUMENT = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'M'),
  K2PG_INVALID_XML_CONTENT = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'N'),
  K2PG_INVALID_XML_COMMENT = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'S'),
  K2PG_INVALID_XML_PROCESSING_INSTRUCTION = K2PG_MAKE_SQLSTATE('2', '2', '0', '0', 'T'),

  /* Class 23 - Integrity Constraint Violation */
  K2PG_INTEGRITY_CONSTRAINT_VIOLATION = K2PG_MAKE_SQLSTATE('2', '3', '0', '0', '0'),
  K2PG_RESTRICT_VIOLATION = K2PG_MAKE_SQLSTATE('2', '3', '0', '0', '1'),
  K2PG_NOT_NULL_VIOLATION = K2PG_MAKE_SQLSTATE('2', '3', '5', '0', '2'),
  K2PG_FOREIGN_KEY_VIOLATION = K2PG_MAKE_SQLSTATE('2', '3', '5', '0', '3'),
  K2PG_UNIQUE_VIOLATION = K2PG_MAKE_SQLSTATE('2', '3', '5', '0', '5'),
  K2PG_CHECK_VIOLATION = K2PG_MAKE_SQLSTATE('2', '3', '5', '1', '4'),
  K2PG_EXCLUSION_VIOLATION = K2PG_MAKE_SQLSTATE('2', '3', 'P', '0', '1'),

  /* Class 24 - Invalid Cursor State */
  K2PG_INVALID_CURSOR_STATE = K2PG_MAKE_SQLSTATE('2', '4', '0', '0', '0'),

  /* Class 25 - Invalid Transaction State */
  K2PG_INVALID_TRANSACTION_STATE = K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '0'),
  K2PG_ACTIVE_SQL_TRANSACTION = K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '1'),
  K2PG_BRANCH_TRANSACTION_ALREADY_ACTIVE = K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '2'),
  K2PG_HELD_CURSOR_REQUIRES_SAME_ISOLATION_LEVEL =
      K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '8'),
  K2PG_INAPPROPRIATE_ACCESS_MODE_FOR_BRANCH_TRANSACTION =
      K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '3'),
  K2PG_INAPPROPRIATE_ISOLATION_LEVEL_FOR_BRANCH_TRANSACTION =
      K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '4'),
  K2PG_NO_ACTIVE_SQL_TRANSACTION_FOR_BRANCH_TRANSACTION =
      K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '5'),
  K2PG_READ_ONLY_SQL_TRANSACTION = K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '6'),
  K2PG_SCHEMA_AND_DATA_STATEMENT_MIXING_NOT_SUPPORTED =
      K2PG_MAKE_SQLSTATE('2', '5', '0', '0', '7'),
  K2PG_NO_ACTIVE_SQL_TRANSACTION = K2PG_MAKE_SQLSTATE('2', '5', 'P', '0', '1'),
  K2PG_IN_FAILED_SQL_TRANSACTION = K2PG_MAKE_SQLSTATE('2', '5', 'P', '0', '2'),
  K2PG_IDLE_IN_TRANSACTION_SESSION_TIMEOUT = K2PG_MAKE_SQLSTATE('2', '5', 'P', '0', '3'),

  /* Class 26 - Invalid SQL Statement Name */
  K2PG_INVALID_SQL_STATEMENT_NAME = K2PG_MAKE_SQLSTATE('2', '6', '0', '0', '0'),

  /* Class 27 - Triggered Data Change Violation */
  K2PG_TRIGGERED_DATA_CHANGE_VIOLATION = K2PG_MAKE_SQLSTATE('2', '7', '0', '0', '0'),

  /* Class 28 - Invalid Authorization Specification */
  K2PG_INVALID_AUTHORIZATION_SPECIFICATION = K2PG_MAKE_SQLSTATE('2', '8', '0', '0', '0'),
  K2PG_INVALID_PASSWORD = K2PG_MAKE_SQLSTATE('2', '8', 'P', '0', '1'),

  /* Class 2B - Dependent Privilege Descriptors Still Exist */
  K2PG_DEPENDENT_PRIVILEGE_DESCRIPTORS_STILL_EXIST =
      K2PG_MAKE_SQLSTATE('2', 'B', '0', '0', '0'),
  K2PG_DEPENDENT_OBJECTS_STILL_EXIST = K2PG_MAKE_SQLSTATE('2', 'B', 'P', '0', '1'),

  /* Class 2D - Invalid Transaction Termination */
  K2PG_INVALID_TRANSACTION_TERMINATION = K2PG_MAKE_SQLSTATE('2', 'D', '0', '0', '0'),

  /* Class 2F - SQL Routine Exception */
  K2PG_SQL_ROUTINE_EXCEPTION = K2PG_MAKE_SQLSTATE('2', 'F', '0', '0', '0'),
  K2PG_S_R_E_FUNCTION_EXECUTED_NO_RETURN_STATEMENT =
      K2PG_MAKE_SQLSTATE('2', 'F', '0', '0', '5'),
  K2PG_S_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED = K2PG_MAKE_SQLSTATE('2', 'F', '0', '0', '2'),
  K2PG_S_R_E_PROHIBITED_SQL_STATEMENT_ATTEMPTED = K2PG_MAKE_SQLSTATE('2', 'F', '0', '0', '3'),
  K2PG_S_R_E_READING_SQL_DATA_NOT_PERMITTED = K2PG_MAKE_SQLSTATE('2', 'F', '0', '0', '4'),

  /* Class 34 - Invalid Cursor Name */
  K2PG_INVALID_CURSOR_NAME = K2PG_MAKE_SQLSTATE('3', '4', '0', '0', '0'),

  /* Class 38 - External Routine Exception */
  K2PG_EXTERNAL_ROUTINE_EXCEPTION = K2PG_MAKE_SQLSTATE('3', '8', '0', '0', '0'),
  K2PG_E_R_E_CONTAINING_SQL_NOT_PERMITTED = K2PG_MAKE_SQLSTATE('3', '8', '0', '0', '1'),
  K2PG_E_R_E_MODIFYING_SQL_DATA_NOT_PERMITTED = K2PG_MAKE_SQLSTATE('3', '8', '0', '0', '2'),
  K2PG_E_R_E_PROHIBITED_SQL_STATEMENT_ATTEMPTED = K2PG_MAKE_SQLSTATE('3', '8', '0', '0', '3'),
  K2PG_E_R_E_READING_SQL_DATA_NOT_PERMITTED = K2PG_MAKE_SQLSTATE('3', '8', '0', '0', '4'),

  /* Class 39 - External Routine Invocation Exception */
  K2PG_EXTERNAL_ROUTINE_INVOCATION_EXCEPTION = K2PG_MAKE_SQLSTATE('3', '9', '0', '0', '0'),
  K2PG_E_R_I_E_INVALID_SQLSTATE_RETURNED = K2PG_MAKE_SQLSTATE('3', '9', '0', '0', '1'),
  K2PG_E_R_I_E_NULL_VALUE_NOT_ALLOWED = K2PG_MAKE_SQLSTATE('3', '9', '0', '0', '4'),
  K2PG_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED = K2PG_MAKE_SQLSTATE('3', '9', 'P', '0', '1'),
  K2PG_E_R_I_E_SRF_PROTOCOL_VIOLATED = K2PG_MAKE_SQLSTATE('3', '9', 'P', '0', '2'),
  K2PG_E_R_I_E_EVENT_TRIGGER_PROTOCOL_VIOLATED = K2PG_MAKE_SQLSTATE('3', '9', 'P', '0', '3'),

  /* Class 3B - Savepoint Exception */
  K2PG_SAVEPOINT_EXCEPTION = K2PG_MAKE_SQLSTATE('3', 'B', '0', '0', '0'),
  K2PG_S_E_INVALID_SPECIFICATION = K2PG_MAKE_SQLSTATE('3', 'B', '0', '0', '1'),

  /* Class 3D - Invalid Catalog Name */
  K2PG_INVALID_CATALOG_NAME = K2PG_MAKE_SQLSTATE('3', 'D', '0', '0', '0'),

  /* Class 3F - Invalid Schema Name */
  K2PG_INVALID_SCHEMA_NAME = K2PG_MAKE_SQLSTATE('3', 'F', '0', '0', '0'),

  /* Class 40 - Transaction Rollback */
  K2PG_TRANSACTION_ROLLBACK = K2PG_MAKE_SQLSTATE('4', '0', '0', '0', '0'),
  K2PG_T_R_INTEGRITY_CONSTRAINT_VIOLATION = K2PG_MAKE_SQLSTATE('4', '0', '0', '0', '2'),
  K2PG_T_R_SERIALIZATION_FAILURE = K2PG_MAKE_SQLSTATE('4', '0', '0', '0', '1'),
  K2PG_T_R_STATEMENT_COMPLETION_UNKNOWN = K2PG_MAKE_SQLSTATE('4', '0', '0', '0', '3'),
  K2PG_T_R_DEADLOCK_DETECTED = K2PG_MAKE_SQLSTATE('4', '0', 'P', '0', '1'),

  /* Class 42 - Syntax Error or Access Rule Violation */
  K2PG_SYNTAX_ERROR_OR_ACCESS_RULE_VIOLATION = K2PG_MAKE_SQLSTATE('4', '2', '0', '0', '0'),
  K2PG_SYNTAX_ERROR = K2PG_MAKE_SQLSTATE('4', '2', '6', '0', '1'),
  K2PG_INSUFFICIENT_PRIVILEGE = K2PG_MAKE_SQLSTATE('4', '2', '5', '0', '1'),
  K2PG_CANNOT_COERCE = K2PG_MAKE_SQLSTATE('4', '2', '8', '4', '6'),
  K2PG_GROUPING_ERROR = K2PG_MAKE_SQLSTATE('4', '2', '8', '0', '3'),
  K2PG_WINDOWING_ERROR = K2PG_MAKE_SQLSTATE('4', '2', 'P', '2', '0'),
  K2PG_INVALID_RECURSION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '9'),
  K2PG_INVALID_FOREIGN_KEY = K2PG_MAKE_SQLSTATE('4', '2', '8', '3', '0'),
  K2PG_INVALID_NAME = K2PG_MAKE_SQLSTATE('4', '2', '6', '0', '2'),
  K2PG_NAME_TOO_LONG = K2PG_MAKE_SQLSTATE('4', '2', '6', '2', '2'),
  K2PG_RESERVED_NAME = K2PG_MAKE_SQLSTATE('4', '2', '9', '3', '9'),
  K2PG_DATATYPE_MISMATCH = K2PG_MAKE_SQLSTATE('4', '2', '8', '0', '4'),
  K2PG_INDETERMINATE_DATATYPE = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '8'),
  K2PG_COLLATION_MISMATCH = K2PG_MAKE_SQLSTATE('4', '2', 'P', '2', '1'),
  K2PG_INDETERMINATE_COLLATION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '2', '2'),
  K2PG_WRONG_OBJECT_TYPE = K2PG_MAKE_SQLSTATE('4', '2', '8', '0', '9'),
  K2PG_GENERATED_ALWAYS = K2PG_MAKE_SQLSTATE('4', '2', '8', 'C', '9'),
  K2PG_UNDEFINED_COLUMN = K2PG_MAKE_SQLSTATE('4', '2', '7', '0', '3'),
  K2PG_UNDEFINED_CURSOR = K2PG_MAKE_SQLSTATE('3', '4', '0', '0', '0'),
  K2PG_UNDEFINED_DATABASE = K2PG_MAKE_SQLSTATE('3', 'D', '0', '0', '0'),
  K2PG_UNDEFINED_FUNCTION = K2PG_MAKE_SQLSTATE('4', '2', '8', '8', '3'),
  K2PG_UNDEFINED_PSTATEMENT = K2PG_MAKE_SQLSTATE('2', '6', '0', '0', '0'),
  K2PG_UNDEFINED_SCHEMA = K2PG_MAKE_SQLSTATE('3', 'F', '0', '0', '0'),
  K2PG_UNDEFINED_TABLE = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '1'),
  K2PG_UNDEFINED_PARAMETER = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '2'),
  K2PG_UNDEFINED_OBJECT = K2PG_MAKE_SQLSTATE('4', '2', '7', '0', '4'),
  K2PG_DUPLICATE_COLUMN = K2PG_MAKE_SQLSTATE('4', '2', '7', '0', '1'),
  K2PG_DUPLICATE_CURSOR = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '3'),
  K2PG_DUPLICATE_DATABASE = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '4'),
  K2PG_DUPLICATE_FUNCTION = K2PG_MAKE_SQLSTATE('4', '2', '7', '2', '3'),
  K2PG_DUPLICATE_PSTATEMENT = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '5'),
  K2PG_DUPLICATE_SCHEMA = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '6'),
  K2PG_DUPLICATE_TABLE = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '7'),
  K2PG_DUPLICATE_ALIAS = K2PG_MAKE_SQLSTATE('4', '2', '7', '1', '2'),
  K2PG_DUPLICATE_OBJECT = K2PG_MAKE_SQLSTATE('4', '2', '7', '1', '0'),
  K2PG_AMBIGUOUS_COLUMN = K2PG_MAKE_SQLSTATE('4', '2', '7', '0', '2'),
  K2PG_AMBIGUOUS_FUNCTION = K2PG_MAKE_SQLSTATE('4', '2', '7', '2', '5'),
  K2PG_AMBIGUOUS_PARAMETER = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '8'),
  K2PG_AMBIGUOUS_ALIAS = K2PG_MAKE_SQLSTATE('4', '2', 'P', '0', '9'),
  K2PG_INVALID_COLUMN_REFERENCE = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '0'),
  K2PG_INVALID_COLUMN_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', '6', '1', '1'),
  K2PG_INVALID_CURSOR_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '1'),
  K2PG_INVALID_DATABASE_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '2'),
  K2PG_INVALID_FUNCTION_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '3'),
  K2PG_INVALID_PSTATEMENT_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '4'),
  K2PG_INVALID_SCHEMA_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '5'),
  K2PG_INVALID_TABLE_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '6'),
  K2PG_INVALID_OBJECT_DEFINITION = K2PG_MAKE_SQLSTATE('4', '2', 'P', '1', '7'),

  /* Class 44 - WITH CHECK OPTION Violation */
  K2PG_WITH_CHECK_OPTION_VIOLATION = K2PG_MAKE_SQLSTATE('4', '4', '0', '0', '0'),

  /* Class 53 - Insufficient Resources */
  K2PG_INSUFFICIENT_RESOURCES = K2PG_MAKE_SQLSTATE('5', '3', '0', '0', '0'),
  K2PG_DISK_FULL = K2PG_MAKE_SQLSTATE('5', '3', '1', '0', '0'),
  K2PG_OUT_OF_MEMORY = K2PG_MAKE_SQLSTATE('5', '3', '2', '0', '0'),
  K2PG_TOO_MANY_CONNECTIONS = K2PG_MAKE_SQLSTATE('5', '3', '3', '0', '0'),
  K2PG_CONFIGURATION_LIMIT_EXCEEDED = K2PG_MAKE_SQLSTATE('5', '3', '4', '0', '0'),

  /* Class 54 - Program Limit Exceeded */
  K2PG_PROGRAM_LIMIT_EXCEEDED = K2PG_MAKE_SQLSTATE('5', '4', '0', '0', '0'),
  K2PG_STATEMENT_TOO_COMPLEX = K2PG_MAKE_SQLSTATE('5', '4', '0', '0', '1'),
  K2PG_TOO_MANY_COLUMNS = K2PG_MAKE_SQLSTATE('5', '4', '0', '1', '1'),
  K2PG_TOO_MANY_ARGUMENTS = K2PG_MAKE_SQLSTATE('5', '4', '0', '2', '3'),

  /* Class 55 - Object Not In Prerequisite State */
  K2PG_OBJECT_NOT_IN_PREREQUISITE_STATE = K2PG_MAKE_SQLSTATE('5', '5', '0', '0', '0'),
  K2PG_OBJECT_IN_USE = K2PG_MAKE_SQLSTATE('5', '5', '0', '0', '6'),
  K2PG_CANT_CHANGE_RUNTIME_PARAM = K2PG_MAKE_SQLSTATE('5', '5', 'P', '0', '2'),
  K2PG_LOCK_NOT_AVAILABLE = K2PG_MAKE_SQLSTATE('5', '5', 'P', '0', '3'),

  /* Class 57 - Operator Intervention */
  K2PG_OPERATOR_INTERVENTION = K2PG_MAKE_SQLSTATE('5', '7', '0', '0', '0'),
  K2PG_QUERY_CANCELED = K2PG_MAKE_SQLSTATE('5', '7', '0', '1', '4'),
  K2PG_ADMIN_SHUTDOWN = K2PG_MAKE_SQLSTATE('5', '7', 'P', '0', '1'),
  K2PG_CRASH_SHUTDOWN = K2PG_MAKE_SQLSTATE('5', '7', 'P', '0', '2'),
  K2PG_CANNOT_CONNECT_NOW = K2PG_MAKE_SQLSTATE('5', '7', 'P', '0', '3'),
  K2PG_DATABASE_DROPPED = K2PG_MAKE_SQLSTATE('5', '7', 'P', '0', '4'),

  /* Class 58 - System Error (errors external to PostgreSQL itself) */
  K2PG_SYSTEM_ERROR = K2PG_MAKE_SQLSTATE('5', '8', '0', '0', '0'),
  K2PG_IO_ERROR = K2PG_MAKE_SQLSTATE('5', '8', '0', '3', '0'),
  K2PG_UNDEFINED_FILE = K2PG_MAKE_SQLSTATE('5', '8', 'P', '0', '1'),
  K2PG_DUPLICATE_FILE = K2PG_MAKE_SQLSTATE('5', '8', 'P', '0', '2'),

  /* Class 72 - Snapshot Failure */
  K2PG_SNAPSHOT_TOO_OLD = K2PG_MAKE_SQLSTATE('7', '2', '0', '0', '0'),

  /* Class F0 - Configuration File Error */
  K2PG_CONFIG_FILE_ERROR = K2PG_MAKE_SQLSTATE('F', '0', '0', '0', '0'),
  K2PG_LOCK_FILE_EXISTS = K2PG_MAKE_SQLSTATE('F', '0', '0', '0', '1'),

  /* Class HV - Foreign Data Wrapper Error (SQL/MED) */
  K2PG_FDW_ERROR = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '0'),
  K2PG_FDW_COLUMN_NAME_NOT_FOUND = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '5'),
  K2PG_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '2'),
  K2PG_FDW_FUNCTION_SEQUENCE_ERROR = K2PG_MAKE_SQLSTATE('H', 'V', '0', '1', '0'),
  K2PG_FDW_INCONSISTENT_DESCRIPTOR_INFORMATION = K2PG_MAKE_SQLSTATE('H', 'V', '0', '2', '1'),
  K2PG_FDW_INVALID_ATTRIBUTE_VALUE = K2PG_MAKE_SQLSTATE('H', 'V', '0', '2', '4'),
  K2PG_FDW_INVALID_COLUMN_NAME = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '7'),
  K2PG_FDW_INVALID_COLUMN_NUMBER = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '8'),
  K2PG_FDW_INVALID_DATA_TYPE = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '4'),
  K2PG_FDW_INVALID_DATA_TYPE_DESCRIPTORS = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '6'),
  K2PG_FDW_INVALID_DESCRIPTOR_FIELD_IDENTIFIER = K2PG_MAKE_SQLSTATE('H', 'V', '0', '9', '1'),
  K2PG_FDW_INVALID_HANDLE = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'B'),
  K2PG_FDW_INVALID_OPTION_INDEX = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'C'),
  K2PG_FDW_INVALID_OPTION_NAME = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'D'),
  K2PG_FDW_INVALID_STRING_LENGTH_OR_BUFFER_LENGTH = K2PG_MAKE_SQLSTATE('H', 'V', '0', '9', '0'),
  K2PG_FDW_INVALID_STRING_FORMAT = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'A'),
  K2PG_FDW_INVALID_USE_OF_NULL_POINTER = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '9'),
  K2PG_FDW_TOO_MANY_HANDLES = K2PG_MAKE_SQLSTATE('H', 'V', '0', '1', '4'),
  K2PG_FDW_OUT_OF_MEMORY = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', '1'),
  K2PG_FDW_NO_SCHEMAS = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'P'),
  K2PG_FDW_OPTION_NAME_NOT_FOUND = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'J'),
  K2PG_FDW_REPLY_HANDLE = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'K'),
  K2PG_FDW_SCHEMA_NOT_FOUND = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'Q'),
  K2PG_FDW_TABLE_NOT_FOUND = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'R'),
  K2PG_FDW_UNABLE_TO_CREATE_EXECUTION = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'L'),
  K2PG_FDW_UNABLE_TO_CREATE_REPLY = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'M'),
  K2PG_FDW_UNABLE_TO_ESTABLISH_CONNECTION = K2PG_MAKE_SQLSTATE('H', 'V', '0', '0', 'N'),

  /* Class P0 - PL/pgSQL Error */
  K2PG_PLPGSQL_ERROR = K2PG_MAKE_SQLSTATE('P', '0', '0', '0', '0'),
  K2PG_RAISE_EXCEPTION = K2PG_MAKE_SQLSTATE('P', '0', '0', '0', '1'),
  K2PG_NO_DATA_FOUND = K2PG_MAKE_SQLSTATE('P', '0', '0', '0', '2'),
  K2PG_TOO_MANY_ROWS = K2PG_MAKE_SQLSTATE('P', '0', '0', '0', '3'),
  K2PG_ASSERT_FAILURE = K2PG_MAKE_SQLSTATE('P', '0', '0', '0', '4'),

  /* Class XX - Internal Error */
  K2PG_INTERNAL_ERROR = K2PG_MAKE_SQLSTATE('X', 'X', '0', '0', '0'),
  K2PG_DATA_CORRUPTED = K2PG_MAKE_SQLSTATE('X', 'X', '0', '0', '1'),
  K2PG_INDEX_CORRUPTED = K2PG_MAKE_SQLSTATE('X', 'X', '0', '0', '2'),
};

#undef K2PG_MAKE_SQLSTATE
#undef K2PG_PGSIXBIT

inline std::string ToString(K2PgErrorCode code) {
  std::string result;
  result.resize(5);
  uint32_t u32_code = static_cast<uint32_t>(code);
  for (int i = 0; i < 5; ++i) {
    result[i] = '0' + ((u32_code >> (i * 6)) & 0x3f);
  }
  return result;
}

}  // namespace k2pg
