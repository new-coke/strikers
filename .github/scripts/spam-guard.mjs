#!/usr/bin/env node
// Usage: spam-guard.mjs event|sweep. GH_TOKEN is a repo owner's PAT with the repo and user scopes.

import { readFileSync } from "node:fs";

const API = "https://api.github.com";
const TOKEN = process.env.GH_TOKEN || "";
const DRY_RUN = process.env.DRY_RUN === "1" || process.env.DRY_RUN === "true";
// The sockpuppet comments land within a minute of the issue.
const SETTLE_SECONDS = 60;
const THRESHOLD = 4;
const NEW_ACCOUNT_DAYS = 90;
const TRUSTED = new Set(["OWNER", "MEMBER", "COLLABORATOR"]);

const [OWNER, REPO] = (process.env.GITHUB_REPOSITORY || "").split("/");
if (!OWNER || !REPO) {
  console.error("GITHUB_REPOSITORY must be set to owner/repo");
  process.exit(2);
}

const PHRASES = [
  /\[\s*install\s*&\s*fix\s*\]/i,
  /fix\s*&\s*install/i,
  /no\s+survey/i,
  /no\s+password/i,
  /\bVT\s*0\s*\/\s*\d+/i,
  /one[\s-]click/i,
  /dll\s+errors?/i,
  /registry\s+corruption/i,
  /license\s*\/\s*activation/i,
  /working\s+(january|february|march|april|may|june|july|august|september|october|november|december)\s+20\d\d/i,
  /just\s+open\s+the\s+repo\s+and\s+run/i,
];
const LURE_REPO = /fix|install|crack|patch|loader|unlock|setup|repair/i;
const PLATFORM =
  /\b(windows|win\s?1[01]|mac|macos|macbook|os\s?x|linux|ubuntu|fedora|debian|arch|mint|bazzite|appimage|steam\s?deck|steamos|switch)\b/i;
const VERSION = /\bv?\d+\.\d+\.\d+\b/;
const ASK_PLATFORM = "which platform (Windows, macOS, Linux, Steam Deck or Nintendo Switch)";
const ASK_VERSION = "which version (the release you downloaded, such as v2.0.0)";

async function gh(path, { method = "GET", body } = {}) {
  const res = await fetch(API + path, {
    method,
    headers: {
      Accept: "application/vnd.github+json",
      "X-GitHub-Api-Version": "2022-11-28",
      ...(TOKEN && { Authorization: `Bearer ${TOKEN}` }),
      ...(body && { "Content-Type": "application/json" }),
    },
    body: body && JSON.stringify(body),
  });
  if (res.status === 204) return null;
  const data = await res.json().catch(() => null);
  if (!res.ok) {
    const err = new Error(`${method} ${path} -> ${res.status} ${data?.message ?? ""}`);
    err.status = res.status;
    throw err;
  }
  return data;
}

function parseUrl(s) {
  try {
    return new URL(s);
  } catch {
    return null;
  }
}

function scoreText(text) {
  const reasons = [];
  let score = 0;

  for (const [, label, href] of text.matchAll(/\[([^\]]+)\]\(([^)\s]+)\)/g)) {
    const shown = parseUrl(label);
    const real = parseUrl(href);
    if (!real) continue;
    // The lure shows a link to this repo whose href goes to another account's repo.
    if (shown && (shown.host !== real.host || shown.pathname.replace(/\/$/, "") !== real.pathname.replace(/\/$/, ""))) {
      score += 3;
      reasons.push(`deceptive link: shows ${label} but goes to ${href}`);
    }
    const [, linkOwner, linkRepo] = real.host === "github.com" ? real.pathname.split("/") : [];
    if (linkOwner && linkOwner.toLowerCase() !== OWNER.toLowerCase() && LURE_REPO.test(linkRepo || "")) {
      score += 2;
      reasons.push(`links to external "fix" repo ${linkOwner}/${linkRepo}`);
    }
  }

  for (const re of PHRASES) {
    if (re.test(text)) {
      score += 1;
      reasons.push(`phrase ${re}`);
    }
  }
  return { score, reasons };
}

async function judge(issue) {
  if (TRUSTED.has(issue.author_association)) return { spam: false, score: 0, reasons: [] };
  let { score, reasons } = scoreText(`${issue.title}\n${issue.body ?? ""}`);
  if (score === THRESHOLD - 1) {
    const user = await gh(`/users/${issue.user.login}`);
    const age = (Date.now() - Date.parse(user.created_at)) / 86_400_000;
    if (age < NEW_ACCOUNT_DAYS) {
      score += 1;
      reasons.push(`author account is ${Math.floor(age)} days old`);
    }
  }
  return { spam: score >= THRESHOLD, score, reasons };
}

async function block(login) {
  if (DRY_RUN) return console.log(`  [dry run] would block ${login}`);
  try {
    await gh(`/user/blocks/${login}`, { method: "PUT" });
    console.log(`  blocked ${login}`);
  } catch (e) {
    if (e.status !== 422) throw e;
    console.log(`  ${login} already blocked`);
  }
}

// 404 and 410 mean another run deleted the issue first.
async function commenters(number) {
  try {
    const comments = await gh(`/repos/${OWNER}/${REPO}/issues/${number}/comments?per_page=100`);
    return [...new Set(comments.filter((c) => !TRUSTED.has(c.author_association)).map((c) => c.user.login))];
  } catch (e) {
    if (e.status === 404 || e.status === 410) return [];
    throw e;
  }
}

async function deleteIssue(issue) {
  if (DRY_RUN) return console.log(`  [dry run] would delete #${issue.number}`);
  const res = await gh("/graphql", {
    method: "POST",
    body: { query: "mutation($id: ID!) { deleteIssue(input: {issueId: $id}) { clientMutationId } }", variables: { id: issue.node_id } },
  });
  const notFound = res.errors?.every((e) => e.type === "NOT_FOUND");
  if (res.errors && !notFound) throw new Error(`deleteIssue #${issue.number}: ${JSON.stringify(res.errors)}`);
  console.log(notFound ? `  #${issue.number} already gone` : `  deleted #${issue.number}`);
}

async function purge(issue, settle) {
  await block(issue.user.login);
  if (settle && !DRY_RUN) {
    console.log(`  waiting ${SETTLE_SECONDS}s for sockpuppet comments`);
    await new Promise((r) => setTimeout(r, SETTLE_SECONDS * 1000));
  }
  for (const login of await commenters(issue.number)) await block(login);
  await deleteIssue(issue);
}

function report(issue, verdict) {
  const tag = verdict.spam ? "SPAM" : "ok  ";
  console.log(`${tag} #${issue.number} (score ${verdict.score}) ${issue.user.login}: ${issue.title}`);
  if (verdict.spam) for (const r of verdict.reasons) console.log(`    - ${r}`);
}

async function askDetails(issue) {
  const text = `${issue.title}\n${issue.body ?? ""}`;
  const asks = [!PLATFORM.test(text) && ASK_PLATFORM, !VERSION.test(text) && ASK_VERSION].filter(Boolean);
  if (!asks.length) return;
  const body = `Thanks for the report. Could you say ${asks.join(" and ")} you're running?`;
  if (DRY_RUN) return console.log(`  [dry run] would ask #${issue.number}: ${body}`);
  await gh(`/repos/${OWNER}/${REPO}/issues/${issue.number}/comments`, { method: "POST", body: { body } });
  console.log(`  asked #${issue.number}: ${body}`);
}

async function runEvent() {
  const { action, issue, comment } = JSON.parse(readFileSync(process.env.GITHUB_EVENT_PATH, "utf8"));
  const verdict = await judge(issue);
  report(issue, verdict);
  if (!verdict.spam) {
    if (action === "opened" && !comment) await askDetails(issue);
    return;
  }
  // The issue run deletes after the settle window; a comment run blocks a commenter who posts after it.
  if (comment) await block(comment.user.login);
  else await purge(issue, true);
}

async function runSweep() {
  for (let page = 1; ; page++) {
    const issues = await gh(`/repos/${OWNER}/${REPO}/issues?state=all&per_page=100&page=${page}`);
    for (const issue of issues) {
      if (issue.pull_request) continue;
      const verdict = await judge(issue);
      report(issue, verdict);
      if (verdict.spam) await purge(issue, false);
    }
    if (issues.length < 100) break;
  }
}

if (DRY_RUN) console.log("DRY RUN: nothing will be blocked or deleted\n");
await (process.argv[2] === "sweep" ? runSweep() : runEvent());
