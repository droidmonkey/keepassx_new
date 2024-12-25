### Reporting Security Issues

The KeePassXC team takes security vulnerabilities very seriously and appreciates your responsible disclosure efforts. We will make every effort to acknowledge your contributions and handle them promptly.

To report a security issue, please use one of the following methods:

- **GitHub Security Advisory:** Use the ["Report a Vulnerability"](https://github.com/keepassxreboot/keepassxc/security/advisories/new) tab on our GitHub repository.
- **Private Matrix Message:** Contact any of the following KeePassXC team members privately (also encrypted):
  - [@droidmonkey_kpxc](https://matrix.to/#/@droidmonkey_kpxc:matrix.org)
  - [@varjolintu](https://matrix.to/#/@varjolintu:matrix.org)
  - [@phoerious](https://matrix.to/#/@phoerious:matrix.org)
- **Send an Email:** Send your report to team@keepassxc.org. We recommend encrypting the email if possible.

Please **DO NOT** use public channels (e.g., GitHub issues, Matrix chat channels) for initial reporting of bona fide security vulnerabilities.

Once you report a security issue, our team will respond with the next steps. After our initial reply, we will keep you informed of the progress towards a fix and full announcement. We may ask for additional information or guidance during this process. If we disagree that your report constitutes a genuine security vulnerability, we will inform you and close the report. Your report may be turned into an issue for further tracking.

If you discover vulnerabilities in third-party modules used by KeePassXC, please report them to the maintainers of the respective modules. If the vulnerability impacts KeePassXC directly, we encourage you to notify us using the above methods. We will validate if the vulnerability is exploitable from KeePassXC code; please note that not all vulnerabilities are actually exploitable and do not constitute an immediate concern for the KeePassXC application.

### Example Security Vulnerabilities

When reporting, please ensure the issue falls under what can be considered a genuine security vulnerability for KeePassXC. Some examples include:

- Unauthorized access to sensitive user data (e.g., passwords).
- Remote code execution or escalation of privileges.
- Bypassing authentication or encryption mechanisms.
- Broken or improperly implemented encryption methods. 

### Counter Examples

The following issues are **not** considered security vulnerabilities:

- Bugs caused by locally modifying the application (e.g., injecting DLLs, altering code).
- Crashes or misbehavior resulting from normal use (report this as a normal issue).
- Vulnerabilities found in third-party modules (should be reported to the module’s maintainers).
  
### CVE Reporting Policy

Please **DO NOT** submit a report to a Common Vulnerabilities and Exposures (CVE) Numbering Authority (CNA) before confirming the security vulnerability with the KeePassXC team. If we do not respond to your report within 30 days, this restriction no longer applies.


### Other Communication

For other inquiries (e.g., developer questions, user questions), please use the public channels on Matrix:
- **User's Channel:** [#keepassxc:mozilla.org](https://matrix.to/#/#keepassxc:mozilla.org)
- **Developer's Channel:** [#keepassxc-dev:mozilla.org](https://matrix.to/#/#keepassxc-dev:mozilla.org)
