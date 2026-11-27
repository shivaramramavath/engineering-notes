# Normalisation: 1NF, 2NF, 3NF, BCNF, and Functional Dependencies

| Metadata | Details |
| :--- | :--- |
| **Topic** | Relational Database Normalisation, Functional Dependencies, 1NF, 2NF, 3NF, BCNF, Anomaly Elimination |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Database Design & Schema Architecture |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [Practical Examples](#practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Notes](#performance--security-notes)
9. [Comparison Matrix](#comparison-matrix)
10. [Interview Questions & Deep Dives](#interview-questions--deep-dives)
11. [Practice Exercises](#practice-exercises)
12. [Summary & Cheat Sheet](#summary--cheat-sheet)
13. [Further Reading](#further-reading)

---

## What & Why

When spreadsheets or denormalized documents are dumped into a database without relational design, three catastrophic database defects occur:
1. **Update Anomalies**: Changing an employee's department requires updating 50,000 duplicate customer rows. If the server crashes mid-update, the database is left in a corrupt state where the employee belongs to two different departments simultaneously.
2. **Insertion Anomalies**: You cannot record the existence of a new warehouse or supplier until a product is physically stored in it, because the warehouse attributes are glued to the product record.
3. **Deletion Anomalies**: Deleting the last order placed by a customer inadvertently wipes out the customer's entire contact history and address from the system.

**Relational Normalisation**—developed by Edgar F. Codd (1970) and refined by Raymond F. Boyce (1974)—is a formal mathematical methodology for decomposing relations into smaller, well-structured tables to eliminate data redundancy and prevent data anomalies.

The famous memory mnemonic summarizes the goal of normalisation:
> *"Every non-key attribute must provide a fact about the key, the whole key, and nothing but the key, so help me Codd."*

Understanding the progression from First Normal Form (1NF) through Boyce-Codd Normal Form (BCNF) is the fundamental requirement for designing transactional databases that guarantee zero data divergence.

---

## Core Concepts

### 1. Functional Dependencies (FDs)
A **Functional Dependency** $X \to Y$ (read: "$X$ functionally determines $Y$") means that within a relation $R$, whenever two tuples have identical values for attribute set $X$, they **must** have identical values for attribute set $Y$.
- Example: `social_security_number -> full_name, date_of_birth`
- **Candidate Key**: A minimal set of attributes that uniquely determines all other attributes in the relation.
- **Prime Attribute**: Any attribute that is part of *at least one* candidate key.

### 2. First Normal Form (1NF): Atomic Values & No Repeating Groups
A table is in 1NF if and only if:
1. Every attribute value is **atomic** (indivisible; no comma-separated text lists like `'Red,Blue,Green'` or nested tables).
2. There are no repeating column groups (`phone_1`, `phone_2`, `phone_3`).
3. Each row is uniquely identifiable by a primary key.

### 3. Second Normal Form (2NF): No Partial Dependencies
A table is in 2NF if and only if:
1. It is in 1NF.
2. Every non-prime attribute is **fully functionally dependent** on the entire candidate key.
- **The Partial Dependency Trap**: If a table has a composite primary key `(order_id, product_id)`, and `product_name` depends *only* on `product_id`, this is a partial dependency. `product_name` must be moved to a separate `products` table!
- *Note*: If a table's candidate key consists of only a single column, it is automatically in 2NF!

### 4. Third Normal Form (3NF): No Transitive Dependencies
A table is in 3NF if and only if:
1. It is in 2NF.
2. No non-prime attribute is **transitively dependent** on a candidate key ($X \to Y$ and $Y \to Z$ where $Z$ is non-prime).
- Example: In `employees(emp_id PK, dept_id, dept_name)`, `emp_id -> dept_id` and `dept_id -> dept_name`. Here, `dept_name` depends on `dept_id`, which is not a candidate key. To reach 3NF, `(dept_id, dept_name)` must be moved into a `departments` table.

### 5. Boyce-Codd Normal Form (BCNF): Strict Determinants
A table is in BCNF if and only if:
- For every non-trivial functional dependency $X \to Y$, **$X$ is a superkey**.
- BCNF is a stricter version of 3NF that eliminates subtle anomalies when a table has **multiple overlapping composite candidate keys**.

---

## How It Works Internally

How does normalisation eliminate anomalies while preserving relational integrity?

```
         [ Unnormalized Flat Table: OrdersSpreadsheet ]
(order_id, order_date, customer_id, customer_name, customer_city, product_id, product_name, price, qty)
                                     │
                 ┌───────────────────┴───────────────────┐
                 ▼                                       ▼
        [ Functional Dependencies ]             [ Anomalies Identified ]
        order_id -> customer_id, date           - Update: Changing customer_name requires
        customer_id -> name, city                 updating 1,000 order rows.
        product_id -> product_name, price       - Delete: Deleting last order wipes customer!
        (order_id, product_id) -> qty           - Insert: Cannot add product without order!
                                     │
                                     ▼
                    [ Lossless BCNF Decomposition ]
        Decompose into 4 discrete relations linked by Foreign Keys:
        1. customers   (customer_id PK, name, city)
        2. products    (product_id PK, product_name, price)
        3. orders       (order_id PK, customer_id FK, order_date)
        4. order_items (order_id FK, product_id FK, qty, unit_price)
```

### Mathematical Invariants:
1. **Lossless Join Guarantee**: When decomposed relations are joined back together using natural joins:
   $$R_1 \bowtie R_2 \bowtie R_3 = R_{\text{original}}$$
   No spurious tuples are created, and no information is lost.
2. **Dependency Preservation**: All functional dependencies can be enforced using primary and unique constraints without requiring cross-table assertions.

---

## Syntax & Usage

### Step-by-Step Normalization DDL Implementation

```sql
-- 1. Normalizing Customer (3NF: Eliminates Transitive Dependency)
CREATE TABLE customers (
    customer_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    customer_name VARCHAR(100) NOT NULL,
    city VARCHAR(50) NOT NULL
);

-- 2. Normalizing Product (2NF: Eliminates Partial Dependency)
CREATE TABLE products (
    product_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    product_name VARCHAR(100) NOT NULL,
    current_price NUMERIC(10,2) NOT NULL CHECK (current_price >= 0)
);

-- 3. Header Order Entity (3NF)
CREATE TABLE orders (
    order_id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    customer_id BIGINT NOT NULL,
    order_date DATE NOT NULL DEFAULT CURRENT_DATE,
    CONSTRAINT fk_orders_customer FOREIGN KEY (customer_id) 
        REFERENCES customers(customer_id)
);
CREATE INDEX idx_orders_customer_id ON orders(customer_id);

-- 4. Line Items Entity (BCNF: Full Key Dependency)
CREATE TABLE order_items (
    order_id BIGINT NOT NULL,
    product_id BIGINT NOT NULL,
    quantity INT NOT NULL CHECK (quantity > 0),
    historical_unit_price NUMERIC(10,2) NOT NULL, -- Point-in-time price snapshot!
    PRIMARY KEY (order_id, product_id),
    CONSTRAINT fk_item_order FOREIGN KEY (order_id) 
        REFERENCES orders(order_id) ON DELETE CASCADE,
    CONSTRAINT fk_item_product FOREIGN KEY (product_id) 
        REFERENCES products(product_id)
);
CREATE INDEX idx_order_items_product_id ON order_items(product_id);
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Check Constraints** | Enforced | Enforced (MySQL 8.0.16+) | Enforced | Enforced |
| **Atomic Enforcement** | Standard scalar types | Standard scalar types | Dynamic typing (Allows non-atomic unless validated) | Standard scalar types |
| **Cross-Table Constraints** | Requires Triggers or `EXCLUDE` | Requires Triggers | Requires Triggers | Requires Triggers |
| **Composite PK Performance**| High (Index-Organized Tuples) | InnoDB clusters on PK directly | Standard B-Tree | Clustered index by default |

---

## Practical Examples

### Example 1: Basic — 1NF Violation & Remediation
Transforming non-atomic comma-separated attributes into First Normal Form.

```sql
-- 0NF (Unnormalized Table):
-- Columns contain repeating comma-separated lists!
CREATE TABLE employee_unnormalized (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(50),
    skills TEXT -- 'Python, SQL, Docker' <- VIOLATES 1NF!
);

-- 1NF Normalized Schema:
CREATE TABLE employees (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(50) NOT NULL
);

CREATE TABLE employee_skills (
    emp_id INT NOT NULL,
    skill_name VARCHAR(50) NOT NULL,
    PRIMARY KEY (emp_id, skill_name),
    FOREIGN KEY (emp_id) REFERENCES employees(emp_id) ON DELETE CASCADE
);
```

---

### Example 2: Real-World — 2NF Violation (Partial Key Dependency)
A university course assignment table where student details depend only on part of the composite primary key.

```sql
-- VIOLATING 2NF:
-- Primary Key: (student_id, course_id)
-- Functional Dependencies:
-- (student_id, course_id) -> grade
-- student_id -> student_name (PARTIAL DEPENDENCY on student_id!)
CREATE TABLE course_grades_bad (
    student_id INT NOT NULL,
    course_id INT NOT NULL,
    student_name VARCHAR(100) NOT NULL, -- PARTIAL DEPENDENCY!
    grade CHAR(2) NOT NULL,
    PRIMARY KEY (student_id, course_id)
);

-- RESOLVED TO 2NF:
CREATE TABLE students (
    student_id INT PRIMARY KEY,
    student_name VARCHAR(100) NOT NULL
);

CREATE TABLE course_grades (
    student_id INT NOT NULL,
    course_id INT NOT NULL,
    grade CHAR(2) NOT NULL,
    PRIMARY KEY (student_id, course_id),
    FOREIGN KEY (student_id) REFERENCES students(student_id)
);
```

---

### Example 3: Edge Case — 3NF Transitive Dependency (The Zip Code Trap)
In address modeling, `zip_code -> city, state`.

```sql
-- VIOLATING 3NF:
-- Primary Key: user_id
-- user_id -> zip_code
-- zip_code -> city, state (TRANSITIVE DEPENDENCY!)
CREATE TABLE user_addresses_bad (
    user_id INT PRIMARY KEY,
    street_address VARCHAR(150) NOT NULL,
    zip_code VARCHAR(10) NOT NULL,
    city VARCHAR(50) NOT NULL,   -- Transitive!
    state VARCHAR(2) NOT NULL    -- Transitive!
);

-- RESOLVED TO 3NF:
CREATE TABLE zip_codes (
    zip_code VARCHAR(10) PRIMARY KEY,
    city VARCHAR(50) NOT NULL,
    state VARCHAR(2) NOT NULL
);

CREATE TABLE user_addresses (
    user_id INT PRIMARY KEY,
    street_address VARCHAR(150) NOT NULL,
    zip_code VARCHAR(10) NOT NULL,
    FOREIGN KEY (zip_code) REFERENCES zip_codes(zip_code)
);
```

---

### Example 4: Failure Mode — BCNF Violation with Overlapping Candidate Keys
Consider a tutoring system:
- A student can have multiple advisors for different subjects.
- For each subject, each advisor tutors only one student.
- Each advisor specializes in only one subject (`advisor -> subject`).

```sql
-- Unnormalized Tutoring:
-- Candidate Keys: (student_id, subject) AND (student_id, advisor_id)
-- Functional Dependency: advisor_id -> subject
-- Here: advisor_id is NOT a superkey, but subject is a prime attribute!
-- This satisfies 3NF, but VIOLATES BCNF!
CREATE TABLE tutoring_bad (
    student_id INT NOT NULL,
    advisor_id INT NOT NULL,
    subject VARCHAR(50) NOT NULL,
    PRIMARY KEY (student_id, subject)
);
-- Anomaly: If advisor "Dr. Smith" stops advising students, we lose the fact
-- that Dr. Smith specializes in 'Physics'! (Deletion Anomaly).

-- RESOLVED TO BCNF:
CREATE TABLE advisor_specialties (
    advisor_id INT PRIMARY KEY,
    subject VARCHAR(50) NOT NULL -- BCNF: advisor_id is a superkey!
);

CREATE TABLE student_advisors (
    student_id INT NOT NULL,
    advisor_id INT NOT NULL,
    PRIMARY KEY (student_id, advisor_id),
    FOREIGN KEY (advisor_id) REFERENCES advisor_specialties(advisor_id)
);
```

---

### Example 5: Optimized — Point-in-Time Snapshot vs Normalization Conflict
In financial and e-commerce transactions, standard normalization dictates that price belongs in the `products` table. However, if product price changes next month, historical invoice totals must **not** change retroactively!

```sql
-- Intentional, correct capture of historical state:
CREATE TABLE historical_invoice_lines (
    invoice_id BIGINT NOT NULL,
    product_id BIGINT NOT NULL,
    quantity INT NOT NULL,
    -- NOT an update anomaly: This records historical fact at transaction time!
    unit_price_at_sale NUMERIC(10,2) NOT NULL,
    PRIMARY KEY (invoice_id, product_id),
    FOREIGN KEY (invoice_id) REFERENCES invoices(invoice_id),
    FOREIGN KEY (product_id) REFERENCES products(product_id)
);
```

---

## Best Practices

1. **Target 3NF as the Baseline for OLTP**: For transactional backend systems, always normalize to at least Third Normal Form (3NF) to eliminate update anomalies.
2. **Distinguish Historical Facts from Redundant Data**: Storing `price_at_purchase` on an invoice line item is *not* a normalisation violation; it is a temporal immutable fact.
3. **Decompose Composite Key Tables Early**: If a table has a composite primary key, inspect every single non-key attribute to verify it depends on *both* keys, not just one.
4. **Use Surrogate Keys to Simplify Foreign Keys**: After decomposing to 3NF, use an auto-generated integer/UUID surrogate primary key to keep foreign key references lightweight.
5. **Never Use Comma-Separated Values in Text Columns**: Always decompose multi-valued attributes into a 1NF child table.

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Storing comma-separated IDs in a `VARCHAR` | Violates 1NF; impossible to index, join, or enforce foreign keys. | Create a junction table with individual foreign keys. |
| 2 | Duplicating parent descriptions in child records | Violates 2NF/3NF; causes update anomalies when description changes. | Keep descriptions in parent table; join via foreign key. |
| 3 | Confusing temporal snapshot with redundancy | Normalizing invoice prices back to `products.price` corrupts accounting when prices change. | Store `unit_price_at_sale` immutably on the order line. |
| 4 | Over-normalizing low-cardinality static attributes | Splitting `(first_name, last_name)` into separate ID lookup tables adds join latency for zero benefit. | Keep scalar attributes in the parent entity. |
| 5 | Deleting child rows causes loss of parent metadata | Deletion anomaly caused by failing to separate independent entities into 3NF. | Decompose entities into dedicated tables. |
| 6 | Forgetting to add Foreign Key indexes after decomposition | Normalising from 1 table to 4 tables increases join frequency; unindexed joins cause performance collapse. | Index every foreign key column created during decomposition. |

---

## Performance & Security Notes

### The Normalization vs Join Performance Tradeoff
- **Normalized Schema (3NF)**:
  - Writes (`INSERT`, `UPDATE`, `DELETE`) are extremely fast: data is written to exactly 1 row in 1 table. Zero lock cascades or redundant updates.
  - Reads (`SELECT`) require joins. If tables are properly indexed, B-Tree join overhead is negligible ($O(\log N)$ seeks).
- **Denormalized Schema**:
  - Reads avoid joins, but writes require updating multiple rows and indexes. Risk of catastrophic data divergence.
- **Rule of Architecture**: Always begin in 3NF. Only denormalize when rigorous profiling proves that join overhead exceeds read latency SLAs.

---

## Comparison Matrix

| Normal Form | Core Requirement | Anomaly Eliminated |
| :--- | :--- | :--- |
| **1NF** | Atomic values, unique rows, no repeating groups | Nested array & unstructured parsing anomalies |
| **2NF** | In 1NF + No partial dependencies on composite keys | Redundant storage of parent attributes in junction tables |
| **3NF** | In 2NF + No transitive dependencies | Update and deletion anomalies on non-candidate keys |
| **BCNF** | Every determinant is a superkey ($X \to Y \implies X$ is superkey) | Anomalies caused by overlapping composite candidate keys |
| **4NF** | Eliminates independent multivalued dependencies | Redundant cross-product combinations |

---

## Interview Questions & Deep Dives

### Q1: What is the difference between 3NF and BCNF?
<details>
<summary>Click to view answer</summary>

Both normal forms eliminate transitive functional dependencies. However:
- In **3NF**, for any non-trivial functional dependency $X \to Y$, either $X$ must be a superkey **OR** $Y$ must be a prime attribute (part of a candidate key).
- In **BCNF** (Boyce-Codd Normal Form), the second condition is eliminated: $X$ **must strictly be a superkey**, regardless of whether $Y$ is prime.
BCNF is strictly stronger than 3NF. A 3NF relation can still exhibit update anomalies if it possesses multiple overlapping composite candidate keys; BCNF eliminates these anomalies.
</details>

### Q2: Recite the classic mnemonic for Third Normal Form.
<details>
<summary>Click to view answer</summary>

> *"Every non-key attribute must provide a fact about the key [1NF], the whole key [2NF], and nothing but the key [3NF], so help me Codd."*
- **The Key**: Every non-key attribute depends on the primary key.
- **The Whole Key**: No attribute depends on only part of a composite primary key (No partial dependencies -> 2NF).
- **Nothing But The Key**: No attribute depends on another non-key attribute (No transitive dependencies -> 3NF).
</details>

### Q3: What is a Partial Functional Dependency?
<details>
<summary>Click to view answer</summary>

A partial functional dependency occurs when a non-prime attribute depends on a **strict subset** of a composite candidate key. For example, in relation $R(A, B, C)$ with candidate key $(A, B)$, if $A \to C$, then $C$ is partially dependent on $(A, B)$. To achieve 2NF, $C$ must be separated into a relation determined solely by $A$.
</details>

### Q4: What is a Transitive Functional Dependency?
<details>
<summary>Click to view answer</summary>

A transitive functional dependency occurs when an attribute depends on another attribute through an intermediate non-key attribute: $A \to B$ and $B \to C$, where $A$ is a candidate key, but $B$ is not. This means $C$ is transitively dependent on $A$. To reach 3NF, $B \to C$ must be removed and placed into a separate relation where $B$ serves as the primary key.
</details>

### Q5: What is a Lossless Decomposition?
<details>
<summary>Click to view answer</summary>

A decomposition of relation $R$ into $R_1$ and $R_2$ is **lossless** if and only if joining $R_1$ and $R_2$ on their shared attributes produces the exact original relation $R$ without creating spurious tuples or omitting genuine data. Formally, by Heath's Theorem, the decomposition is lossless if $(R_1 \cap R_2) \to R_1$ or $(R_1 \cap R_2) \to R_2$ (the intersection of attributes forms a superkey for at least one of the decomposed tables).
</details>

### Q6: Can a table with a single-column Primary Key violate 2NF?
<details>
<summary>Click to view answer</summary>

**No**. 2NF forbids partial dependencies, which by definition can only occur when a candidate key consists of two or more attributes (a composite key). If the primary key is a single column, no proper subset of the key exists; therefore, any 1NF table with a single-column primary key is **automatically in 2NF**.
</details>

### Q7: Why do databases store `price` on both `products` and `order_items`?
<details>
<summary>Click to view answer</summary>

This is not an anomaly; it is an enforcement of **temporal correctness**. 
- `products.price` represents the *current catalog list price* (a mutable attribute).
- `order_items.unit_price` represents the *contractual execution price at the moment of sale* (an immutable historical fact).
If `order_items` did not capture historical price and joined to `products.price`, changing product prices next month would retroactively corrupt the financial ledger of historical sales.
</details>

### Q8: What are Armstrong's Axioms?
<details>
<summary>Click to view answer</summary>

Armstrong's Axioms are a set of sound and complete inference rules used to deduce all functional dependencies on a relational database:
1. **Reflexivity**: If $Y \subseteq X$, then $X \to Y$.
2. **Augmentation**: If $X \to Y$, then $XZ \to YZ$.
3. **Transitivity**: If $X \to Y$ and $Y \to Z$, then $X \to Z$.
Secondary rules derived from these axioms include Union, Decomposition, and Pseudotransitivity.
</details>

---

## Practice Exercises

### Exercise 1: Identify Normal Form Violation
Given `student_exams(student_id, exam_id, student_email, exam_date, score)` with candidate key `(student_id, exam_id)`. Functional dependencies are:
- `(student_id, exam_id) -> score`
- `student_id -> student_email`
- `exam_id -> exam_date`
Identify the normal form of this table and explain why.

<details>
<summary>Click to view solution</summary>

The table is in **1NF**, but **violates 2NF**.

**Reason**: The primary key is composite `(student_id, exam_id)`. Attributes `student_email` and `exam_date` exhibit partial dependencies: `student_email` depends only on `student_id`, and `exam_date` depends only on `exam_id`. To reach 2NF, decompose into:
1. `students(student_id PK, student_email)`
2. `exams(exam_id PK, exam_date)`
3. `exam_scores(student_id PK,FK, exam_id PK,FK, score)`
</details>

---

### Exercise 2: Normalize to 3NF
Given `hospital_wards(patient_id PK, patient_name, ward_id, ward_name, nurse_in_charge)`. FDs:
- `patient_id -> patient_name, ward_id`
- `ward_id -> ward_name, nurse_in_charge`
Decompose this table into 3NF relations.

<details>
<summary>Click to view solution</summary>

```sql
-- Ward Entity (3NF: Eliminates transitive dependency)
CREATE TABLE wards (
    ward_id INT PRIMARY KEY,
    ward_name VARCHAR(50) NOT NULL,
    nurse_in_charge VARCHAR(100) NOT NULL
);

-- Patient Entity (3NF)
CREATE TABLE patients (
    patient_id INT PRIMARY KEY,
    patient_name VARCHAR(100) NOT NULL,
    ward_id INT NOT NULL,
    FOREIGN KEY (ward_id) REFERENCES wards(ward_id)
);
```
</details>

---

### Exercise 3: Prove Lossless Decomposition
Given relation $R(A, B, C)$ with $A \to B$. Decompose $R$ into $R_1(A, B)$ and $R_2(A, C)$ and prove mathematically that the decomposition is lossless.

<details>
<summary>Click to view solution</summary>

By Heath's Theorem, a decomposition into $R_1$ and $R_2$ is lossless if $R_1 \cap R_2 \to R_1$ or $R_1 \cap R_2 \to R_2$.
- $R_1 \cap R_2 = \{A\}$
- Since we are given $A \to B$, and $R_1 = \{A, B\}$, the common attribute set $\{A\}$ functionally determines all attributes of $R_1$ ($\{A\} \to R_1$).
- Therefore, the decomposition is guaranteed to be **lossless**.
</details>

---

### Exercise 4: Spot the 1NF Violation
Review this table and explain how it violates 1NF:
```sql
CREATE TABLE company_locations (
    company_id INT PRIMARY KEY,
    company_name VARCHAR(100),
    headquarters_phone VARCHAR(20),
    secondary_phone VARCHAR(20),
    fax_phone VARCHAR(20)
);
```

<details>
<summary>Click to view solution</summary>

While scalar, having `headquarters_phone`, `secondary_phone`, and `fax_phone` represents a **repeating column group** for the telephone concept. If the company needs a 4th phone number, the physical schema must be altered with DDL. 

To achieve pure 1NF, normalize to:
- `companies(company_id PK, company_name)`
- `company_phones(company_id PK,FK, phone_type PK, phone_number)`
</details>

---

### Exercise 5: BCNF Decomposition of Property Management
Given `property_contracts(property_id, county_name, tax_rate)`. FDs:
- `property_id -> county_name`
- `county_name -> tax_rate`
Decompose into BCNF tables.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE county_tax_rates (
    county_name VARCHAR(50) PRIMARY KEY, -- Superkey in this relation!
    tax_rate NUMERIC(5,4) NOT NULL
);

CREATE TABLE properties (
    property_id INT PRIMARY KEY,
    county_name VARCHAR(50) NOT NULL,
    FOREIGN KEY (county_name) REFERENCES county_tax_rates(county_name)
);
```
</details>

---

### Exercise 6: Evaluate Third Normal Form
A junior developer claims that `employees(emp_id PK, first_name, last_name, full_name)` is in 3NF. Refute this claim.

<details>
<summary>Click to view solution</summary>

The table violates 3NF. `full_name` is transitively dependent on `(first_name, last_name)` because `full_name` is a derived attribute ($first\_name, last\_name \to full\_name$). 

Storing `full_name` introduces an **update anomaly**: if an employee changes their `last_name`, but `full_name` is not updated, the data diverges. `full_name` should be removed and computed via a generated column:
```sql
full_name VARCHAR(200) GENERATED ALWAYS AS (first_name || ' ' || last_name) STORED
```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                NORMALISATION CHEAT SHEET                                |
+-------------------+----------------------------+----------------------------------------+
| Level             | Rule                       | Test Question                          |
+-------------------+----------------------------+----------------------------------------+
| 1NF               | Atomic values, no repeats  | Are there comma lists or array cols?   |
| 2NF               | 1NF + No partial key deps  | Does a column depend on HALF the PK?   |
| 3NF               | 2NF + No transitive deps   | Does a column depend on a NON-KEY col? |
| BCNF              | All determinants superkeys | Is X always a superkey in X -> Y?      |
+-------------------+----------------------------+----------------------------------------+
```

- **Mnemonic**: The Key, the Whole Key, and Nothing But The Key!
- **Default Baseline**: Design in 3NF for all OLTP production systems.
- **Foreign Key Rule**: Always index the foreign keys created by normalisation.

---

## Further Reading
- E.F. Codd: *A Relational Model of Data for Large Shared Data Banks (ACM 1970)*
- Raymond F. Boyce & E.F. Codd: *Recent Investigations in Relational Data Base Systems (1974)*
- C.J. Date: *An Introduction to Database Systems (Normalisation & Functional Dependencies)*
- Joe Celko: *Data and Databases: Concepts in Practice*
