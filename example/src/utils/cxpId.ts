/**
 * CXP ID Generator
 *
 * Generates random IDs similar to CXP format (e.g., "nhmi2jk", "4lj3ufg")
 */

export function generateCxpId(): string {
  const chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
  let id = '';
  for (let i = 0; i < 7; i++) {
    id += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return id;
}
