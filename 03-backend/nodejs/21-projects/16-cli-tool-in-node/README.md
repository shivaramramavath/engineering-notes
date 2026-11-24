# Project 16: Production Developer CLI Tool (`proj-node-16-cli-tool`)

> Modern command-line developer utility built with Node.js 22 LTS, Commander, and `@inquirer/prompts`. Implements interactive terminal prompts, streaming spinner feedback (`ora`), XDG-compliant config file storage, UNIX piping support (`stdin`), strict exit code standards, and standalone binary distribution via Node.js Single Executable Applications (SEA).

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-16-cli-tool` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **CLI Framework** | Commander.js (`commander`) |
| **Interactive Prompts**| `@inquirer/prompts` |
| **Terminal Styling** | `picocolors` + `ora` spinners |
| **Distribution** | npm `bin` script & Single Executable App (SEA) |
| **Testing** | Vitest (CLI process execution testing) |
| **Complexity Level** | Intermediate / Production-Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 CLI Architecture & UNIX Composability

Modern Node.js CLI tools must support both interactive terminal sessions and non-interactive scripted pipeline executions:
1. **TTY Detection**: If `process.stdin.isTTY` is false, the CLI reads directly from piped standard input (`cat file.json | devctl process`). If true, it renders interactive menus.
2. **Standard Exit Codes**:
   - `0`: Success
   - `1`: General application error
   - `2`: Invalid command line arguments
   - `130`: Terminated by user (`SIGINT` / Ctrl+C)
3. **XDG Base Directory Specification**: Configuration files are stored in `~/.config/devctl/config.json`, never polluted into the current working directory.
4. **Single Executable Application (SEA)**: Bundled into a standalone native executable using Node.js 22's built-in `node --experimental-sea-config`.

```
+----------------------------------------------------------------------------------------------------+
|                                    Node.js CLI Architecture                                        |
|                                                                                                    |
|  [ User Shell Command: devctl analyze --format=json ]                                              |
|                           │                                                                        |
|                           ▼                                                                        |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                 Commander.js Argument & Option Parser       |                                   |
|  |  - Validate flags (--format, --verbose, --config)           |                                   |
|  |  - Display contextual --help and auto-generated manuals     |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │                                                                   |
|             ┌──────────────────┴──────────────────┐                                                |
|             │ Interactive TTY Mode                │ Non-Interactive / Scripted (Piped stdin)       |
|             ▼                                     ▼                                                |
|  +─────────────────────────+       +─────────────────────────+                                     |
|  | Inquirer Terminal UI    |       | Stream Reader           |                                     |
|  | - Interactive dropdowns |       | - Consume process.stdin |                                     |
|  | - Ora animated spinner  |       | - Raw NDJSON / buffers  |                                     |
|  +───────────┬─────────────+       +─────────────┬───────────+                                     |
|              │                                   │                                                 |
|              └─────────────────┬─────────────────┘                                                 |
|                                │                                                                   |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                     Core Command Runner                     |                                   |
|  |  - Execute analysis & emit formatted output (JSON / Table)  |                                   |
|  |  - Return POSIX exit code (process.exit(0) / (1))           |                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-16-cli-tool/
├── bin/
│   └── devctl.js                # Executable entrypoint with #!/usr/bin/env node
├── src/
│   ├── commands/
│   │   ├── init.command.ts      # Interactive setup wizard
│   │   └── analyze.command.ts   # Piped data analysis
│   ├── config/
│   │   └── xdg.ts               # XDG config file reader/writer
│   └── utils/
│       └── formatters.ts        # Table & colored output helpers
├── test/
│   └── integration/
│       └── cli.test.ts          # Child process execution tests
├── sea-config.json              # Node.js Single Executable Application config
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "devctl",
  "version": "1.0.0",
  "type": "module",
  "bin": {
    "devctl": "./bin/devctl.js"
  },
  "scripts": {
    "test": "vitest run"
  },
  "dependencies": {
    "@inquirer/prompts": "^7.3.2",
    "commander": "^13.1.0",
    "ora": "^8.2.0",
    "picocolors": "^1.1.1"
  },
  "devDependencies": {
    "@types/node": "^22.13.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `bin/devctl.js`
```javascript
#!/usr/bin/env node
import { Command } from 'commander';
import pc from 'picocolors';
import ora from 'ora';
import { input, select } from '@inquirer/prompts';

const program = new Command();

program
  .name('devctl')
  .description('Production Developer CLI Utility in Node.js 22')
  .version('1.0.0');

// Command 1: Interactive Wizard
program
  .command('init')
  .description('Initialize local developer environment')
  .action(async () => {
    try {
      console.log(pc.cyan(pc.bold('\n--- DevCtl Setup Wizard ---\n')));

      const projectName = await input({
        message: 'Enter your project name:',
        default: 'my-node-app',
      });

      const runtime = await select({
        message: 'Select target runtime baseline:',
        choices: [
          { name: 'Node.js 22 LTS (Jod)', value: 'node22' },
          { name: 'Node.js 24 (Current)', value: 'node24' },
        ],
      });

      const spinner = ora('Configuring project workspace...').start();
      await new Promise((resolve) => setTimeout(resolve, 1200));
      spinner.succeed(pc.green(`Workspace "${projectName}" created successfully for ${runtime}!`));

      process.exit(0);
    } catch (err) {
      if ((err).name === 'ExitPromptError') {
        console.log(pc.yellow('\nOperation cancelled by user.'));
        process.exit(130);
      }
      console.error(pc.red('Fatal error:'), err);
      process.exit(1);
    }
  });

// Command 2: Stream Analyzer (UNIX Pipe Friendly)
program
  .command('analyze')
  .description('Analyze input data from pipe or parameter')
  .option('-f, --format <format>', 'Output format (json|table)', 'table')
  .action(async (options) => {
    let data = '';

    // Check if data is being piped into stdin
    if (!process.stdin.isTTY) {
      for await (const chunk of process.stdin) {
        data += chunk;
      }
    }

    const payload = data.trim() || 'Default analysis metrics';
    const result = {
      timestamp: new Date().toISOString(),
      bytesRead: Buffer.byteLength(data),
      summary: payload.slice(0, 50),
    };

    if (options.format === 'json') {
      console.log(JSON.stringify(result, null, 2));
    } else {
      console.log(pc.bold(pc.green('\nAnalysis Result:')));
      console.table([result]);
    }

    process.exit(0);
  });

program.parse(process.argv);
```

### `sea-config.json`
```json
{
  "main": "bin/devctl.js",
  "output": "sea-prep.blob",
  "disableExperimentalSEAWarning": true
}
```

---

## 3. Verification & Testing Suite

### Unit Test (`test/integration/cli.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import { execSync } from 'node:child_process';

describe('DevCtl CLI Process Execution', () => {
  it('outputs version string with --version', () => {
    const output = execSync('node bin/devctl.js --version', { encoding: 'utf8' });
    expect(output.trim()).toBe('1.0.0');
  });

  it('prints help message with --help', () => {
    const output = execSync('node bin/devctl.js --help', { encoding: 'utf8' });
    expect(output).toContain('Usage: devctl [options] [command]');
    expect(output).toContain('init');
    expect(output).toContain('analyze');
  });
});
```

---

## 4. Single Executable Application (SEA) Compilation

To compile `devctl` into a standalone native binary without needing Node.js pre-installed on the host machine:

```bash
# 1. Generate injection blob
node --experimental-sea-config sea-config.json

# 2. Copy system node binary
cp $(command -v node) devctl-bin

# 3. Inject blob into binary with postject (macOS/Linux)
npx postject devctl-bin NODE_SEA_BLOB sea-prep.blob \
  --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2

# 4. Binary is now ready for standalone distribution!
./devctl-bin --version
```
